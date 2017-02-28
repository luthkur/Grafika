#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <math.h>
#include <termios.h>

int fbfd = 0;                       // Filebuffer Filedescriptor
struct fb_var_screeninfo vinfo;     // Struct Variable Screeninfo
struct fb_fix_screeninfo finfo;     // Struct Fixed Screeninfo
long int screensize = 0;            // Ukuran data framebuffer
char *fbp = 0;                      // Framebuffer di memori internal

int borderwidthu = 10;               // The border width, distance from the actual screenBorder
int borderwidthd = 10;               // The border width, distance from the actual screenBorder
int borderwidthl = 10;               // The border width, distance from the actual screenBorder
int borderwidthr = 10;               // The border width, distance from the actual screenBorder

int xmiddle;
int ymiddle;

float scale = 1;

// UTILITY PROCEDURE----------------------------------------------------------------------------------------- //
int isOverflow(int _x , int _y) {
//Cek apakah kooordinat (x,y) sudah melewati batas layar
    int result;
    if ( _x > vinfo.xres-borderwidthd || _y > vinfo.yres-borderwidthr || _x < borderwidthu-1 || _y < borderwidthl-1 ) {
        result = 1;
    }
    else {
        result = 0;
    }
    return result;
}

void terminate() {
	//Pengakhiran program.
     munmap(fbp, screensize);
     close(fbfd);
}

//read char without print
char getch() {
    char buf = 0;
    struct termios old = {0};
    struct termios new = {0};
    if (tcgetattr(0, &old) < 0)
            perror("tcsetattr()");
    new = old;
    new.c_lflag &= ~ICANON;
    new.c_lflag &= ~ECHO;
    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &new) < 0)
            perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
            perror ("read()");
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
            perror ("tcsetattr ~ICANON");
    return (buf);
}

void drawScreenBorder();

// PROSEDUR RESET LAYAR DAN PEWARNAAN PIXEL----------------------------------------------------------------- //
int plotPixelRGBA(int _x, int _y, int r, int g, int b, int a) {
	//Plot pixel (x,y) dengan warna (r,g,b,a)
	if(!isOverflow(_x,_y)) {
		long int location = (_x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (_y+vinfo.yoffset) * finfo.line_length;
		*(fbp + location) = b;        //blue
		*(fbp + location + 1) = g;    //green
		*(fbp + location + 2) = r;    //red
		*(fbp + location + 3) = a;      //
	}
}

// Check if the pixel in x,y is colored r,g,b,a. Only for 32 bit bpp
int isPixelColor(int _x, int _y, int r, int g, int b, int a) {
	if(!isOverflow(_x,_y)) {
		long int location = (_x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (_y+vinfo.yoffset) * finfo.line_length;
		if((*((unsigned char *)(fbp + location)) == b)&&(*((unsigned char *)(fbp + location + 1)) == g)&&(*((unsigned char *)(fbp + location + 2)) == r)&&(*((unsigned char *)(fbp + location + 3)) == a)) {
			return 1;
		} else return 0;
	} else return 0;
}

void initScreen() {
	 // Mapping framebuffer ke memori internal
     // Buka framebuffer
     fbfd = open("/dev/fb0", O_RDWR);
     if (fbfd == -1) {
         perror("Error: cannot open framebuffer device");
         exit(1);
     }
     printf("The framebuffer device was opened successfully.\n");

     // Ambil data informasi screen
     if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
         perror("Error reading fixed information");
         exit(2);
     }
     if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
         perror("Error reading variable information");
         exit(3);
     }

     // Informasi resolusi, dan bit per pixel
     printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);

     // Hitung ukuran memori untuk menyimpan framebuffer
     screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;

     // Masukkan framebuffer ke memory, untuk kita ubah-ubah
     fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fbfd, 0);
     if ((int)fbp == -1) {
         perror("Error: failed to map framebuffer device to memory");
         exit(4);
     }
     printf("The framebuffer device was mapped to memory successfully.\n");
     
     if (!vinfo.bits_per_pixel == 32) {
		perror("bpp NOT SUPPORTED");
		exit(0);
	 }
	 
     xmiddle = 305;
     ymiddle = 305;
     //borderwidthd = vinfo.xres/2;
     borderwidthd = vinfo.xres - (borderwidthu + 600);
     borderwidthr = vinfo.yres - (borderwidthl + 600);
     // borderwidthr = vinfo.xres/2;
     // borderwidthu = vinfo.xres/2;
     // borderwidthl = vinfo.xres/2;

     //borderwidthd = 200; //ngurangin kanan?
     // borderwidthr = 200; //ngurangin yg bawah
     // borderwidthu = 200; //ngurangin kiri
     // borderwidthl = 200; //ngurangin atas
    
}
void clearScreen() {
	
	//Mewarnai latar belakang screen dengan warna putih
    int x = 0;
    int y = 0;
    for (y = 0; y < vinfo.yres; y++) {
		for (x = 0; x < vinfo.xres; x++) {

			plotPixelRGBA(x,y,0,0,0,0);

        }
	}
	drawScreenBorder();
	
}


// STRUKTUR DATA DAN METODE PENGGAMBARAN GARIS-------------------------------------------------------------- //
typedef struct {

  int x1; int y1; // Koordinat titik pertama garis
  int x2; int y2; // Koordinat titik kedua garis

  // Encoding warna garis dalam RGBA / RGB
  int r; int g; int b; int a;

} Line;

void initLine(Line* l, int xa, int ya, int xb, int yb, int rx, int gx, int bx, int ax) {
	(*l).x1 = xa; (*l).y1 = ya;
	(*l).x2 = xb; (*l).y2 = yb;
	(*l).r = rx; (*l).g = gx; (*l).b = bx; (*l).a = ax;
}

int getClipCode(int x, int y) {
	int code = 0;

	int xmin = borderwidthu-1, xmax = vinfo.xres-borderwidthd;
	int ymin = borderwidthl-1, ymax = vinfo.yres-borderwidthr;

	if (x < xmin)           // to the left of clip window 0001
		code |= 1;
	else if (x > xmax)      // to the right of clip window 0010
		code |= 2;
	if (y < ymin)           // below the clip window 0100
		code |= 4;
	else if (y > ymax)      // above the clip window 1000
		code |= 8;

	return code;
}

int drawLine(Line* l) {
	
	/*if(isOverflow((*l).x1, (*l).y1) && isOverflow((*l).x2, (*l).y2)) {
		return 0; // Do Nothing if both of the endPoint overflowed
		
	} else if(isOverflow((*l).x1, (*l).y1) || isOverflow((*l).x2, (*l).y2)) {
		// If one of the endPoint overflowed
		
		// Memastikan x1 < x2
		if((*l).x1 > (*l).x2) {
			int x,y;
			x = (*l).x1;
			y = (*l).y1;
			(*l).x1 = (*l).x2;
			(*l).y1 = (*l).y2;
			(*l).x2 = x;
			(*l).y2 = y;
		}

		int xlow = borderwidthu-1, xhigh = vinfo.xres-borderwidthd;
		int ylow = borderwidthl-1, yhigh = vinfo.yres-borderwidthr;
		int x1 = (*l).x1, y1 = (*l).y1;
		int x2 = (*l).x2, y2 = (*l).y2;
		int x, y;

		if (x2 > xhigh) {
			//printf("%d %d %d %d %d %d %d\n", xhigh, x1, y2, y1, x2, x1, y1);
			y = ((xhigh - x1) * (y2-y1) / (x2 - x1)) + y1;
			//printf("aaa %d %d\n", x, y);
			if (y < ylow) {
				x = ((ylow - y1) * (x2 - x1) / (y2 - y1)) + x1;
				y = ylow;
				//printf("aaa %d %d\n", x, y);
			} else if (y > yhigh) {
				x  = ((yhigh - y1) * (x2 - x1) / (y2 - y1)) + x1;
				y = yhigh;
				//printf("aaa %d %d\n", x, y);
			} else {
				x = xhigh;
				//printf("aaa %d %d\n", x, y);
			}
			(*l).x2 = x;
			(*l).y2 = y;
			//printf("aaa %d %d\n", x, y);
			return drawLine(l);
		} else if (x1 < xlow) {
			y = ((xlow - x1) * (y2-y1) / (x2 - x1)) + y1;
			if (y < ylow) {
				x  = ((ylow - y1) * (x2 - x1) / (y2 - y1)) + x1;
				y = ylow;
			} else if (y > yhigh) {
				x  = ((yhigh - y1) * (x2 - x1) / (y2 - y1)) + x1;
				y = yhigh;
			} else {
				x = xlow;
			}
			(*l).x1 = x;
			(*l).y1 = y;
			//printf("bbb %d %d\n", x, y);
			return drawLine(l);
		} else {
			if ((y1 < ylow) || (y2 < ylow)) {
				y = ylow;
				x  = ((ylow - y1) * (x2 - x1) / (y2 - y1)) + x1;
			} else if ((y2 > yhigh) || (y1 > yhigh)) {
				y = yhigh;
				x  = ((yhigh - y1) * (x2 - x1) / (y2 - y1)) + x1;
			}

			if ((y1 < ylow) || (y1 > yhigh)) {
				(*l).x1 = x;
				(*l).y1 = y;
			} else if ((y2 < ylow) || (y2 > yhigh)) {
				(*l).x2 = x;
				(*l).y2 = y;
			}
			//printf("ccc %d %d\n", x, y);
			return drawLine(l);
		}
	}*/

	int x1 = (*l).x1, y1 = (*l).y1;
	int x2 = (*l).x2, y2 = (*l).y2;
	int xmin = borderwidthu-1, xmax = vinfo.xres-borderwidthd;
	int ymin = borderwidthl-1, ymax = vinfo.yres-borderwidthr;
	int x, y;

	int code1 = getClipCode(x1, y1);
	int code2 = getClipCode(x2, y2);

	int accept = 0;
	int valid = 1;

	if(isOverflow((*l).x1, (*l).y1) || isOverflow((*l).x2, (*l).y2)) {
		while(1) {
			if (!(code1 | code2)) { // Kedua endpoint di dalam batas, keluar loop & print
				accept = 1;
				break;
			} else if (code1 & code2) { // Kedua endpoint di region yang sama diluar batas
				break;
			} else {

				//Salah satu endpoint ada di luar batas
				int code = code1 ? code1 : code2;

				//Cara perpotongan menggunakan persamaan garis
				if (code & 8) {           // endpoint di atas area clip
					x = x1 + (x2 - x1) * (ymax - y1) / (y2 - y1);
					y = ymax;
				} else if (code & 4) { // endpoint di bawah area clip
					x = x1 + (x2 - x1) * (ymin - y1) / (y2 - y1);
					y = ymin;
				} else if (code & 2) {  // endpoint di sebelah kanan area clip
					y = y1 + (y2 - y1) * (xmax - x1) / (x2 - x1);
					x = xmax;
				} else if (code & 1) {   // endpoint di sebelah kiri area clip
					y = y1 + (y2 - y1) * (xmin - x1) / (x2 - x1);
					x = xmin;
				}

				//Pindahkan point yang ada di luar area ke dalam
				if (code == code1) {
					x1 = x;
					y1 = y;
					code1 = getClipCode(x1, y1);
				} else {
					x2 = x;
					y2 = y;
					code2 = getClipCode(x2, y2);
				}
			}
		}

		if(accept) {
			(*l).x1 = x1;
			(*l).y1 = y1;
			(*l).x2 = x2;
			(*l).y2 = y2;
			return drawLine(l);
		}
	}


	
	int col = 0;
	
	// Coord. of the next point to be displayed
	x = (*l).x1;
	y = (*l).y1;

	// Calculate initial error factor
	int dx = abs((*l).x2 - (*l).x1);
	int dy = abs((*l).y2 - (*l).y1);
	int p = 0;

	// If the absolute gradien is less than 1
	if(dx >= dy) {

		// Memastikan x1 < x2
		if((*l).x1 > (*l).x2) {
			(*l).x1 = (*l).x2;
			(*l).y1 = (*l).y2;
			(*l).x2 = x;
			(*l).y2 = y;
			x = (*l).x1;
			y = (*l).y1;
		}

		// Repeat printing the next pixel until the line is painted
		while(x <= (*l).x2) {

			// Draw the next pixel
			if (!isOverflow(x,y)) {
				plotPixelRGBA(x,y,(*l).r,(*l).g,(*l).b,(*l).a);
			}

			// Calculate the next pixel
			if(p < 0) {
				p = p + 2*dy;
			} else {
				p = p + 2*(dy-dx);
				if((*l).y2 - (*l).y1 > 0) y++;
				else y--;
			}
			x++;

		}

	// If the absolute gradien is more than 1
	} else {

		// Memastikan y1 < y2
		if((*l).y1 > (*l).y2) {
			(*l).x1 = (*l).x2;
			(*l).y1 = (*l).y2;
			(*l).x2 = x;
			(*l).y2 = y;
			x = (*l).x1;
			y = (*l).y1;
		}

		// Repeat printing the next pixel until the line is painted
		while(y <= (*l).y2) {

			// Draw the next pixel
			if (!isOverflow(x,y)) {
				plotPixelRGBA(x,y,(*l).r,(*l).g,(*l).b,(*l).a);
			}

			// Calculate the next pixel
			if(p < 0) {
				p = p + 2*dx;
			} else {
				p = p + 2*(dx-dy);
				if((*l).x2 - (*l).x1 > 0) x++;
				else x--;
			}
			y++;

		}
	
	}
	
	return col;
	
}


// METODE PEWARNAAN UMUM------------------------------------------------------------------------------------ //
void floodFill(int x,int y, int r,int g,int b,int a, int rb,int gb,int bb,int ab) {
// rgba is the color of the fill, rbgbbbab is the color of the border
	if((!isOverflow(x,y)) ) {
		if(!isPixelColor(x,y, r,g,b,a)) {
			if(!isPixelColor(x,y, rb,gb,bb,ab)) {

				plotPixelRGBA(x,y, r,g,b,a);
				floodFill(x+1,y, r,g,b,a, rb,gb,bb,ab);
				floodFill(x-1,y, r,g,b,a, rb,gb,bb,ab);
				floodFill(x,y+1, r,g,b,a, rb,gb,bb,ab);
				floodFill(x,y-1, r,g,b,a, rb,gb,bb,ab);

			}
		}
	}

}


// STRUKTUR DATA DAN METODE PENGGAMBARAN POLYLINE----------------------------------------------------------- //
typedef struct {

	int x[100];
	int y[100];

	// The coord. of this Polyline firepoint
	int xp; int yp;

	int PointCount; // Jumlah end-point dalam polyline, selalu >1

	// Encoding warna outline dalam RGBA / RGB
	int r; int g; int b; int a;

} PolyLine;

void initPolyline(PolyLine* p, int rx, int gx, int bx, int ax) {
// Reset Polyline dan men-set warna garis batas
	(*p).PointCount = 0;
	(*p).r = rx; (*p).g = gx; (*p).b = bx; (*p).a = ax;
}
void addEndPoint(PolyLine* p, int _x, int _y) {
	(*p).x[(*p).PointCount] = _x;
	(*p).y[(*p).PointCount] = _y;
	(*p).PointCount++;
}
void setFirePoint(PolyLine* p, int x, int y) {
	(*p).xp = x;
	(*p).yp = y;
}

void boxPolyline(PolyLine* p, int x1, int y1, int x2, int y2) {
	addEndPoint(p, x1, y1);
	addEndPoint(p, x1, y2);
	addEndPoint(p, x2, y2);
	addEndPoint(p, x2, y1);
	int xp = (x1+x2)/2;
	int yp = (y1+y2)/2;
	setFirePoint(p, xp, yp);
}

int drawPolylineOutline(PolyLine* p) {
	int col = 0;
	int i; Line l;
	for(i=1; i<(*p).PointCount; i++) {
		initLine(&l, (*p).x[i-1], (*p).y[i-1], (*p).x[i], (*p).y[i] ,(*p).r, (*p).g, (*p).b, (*p).a);
		col |= drawLine(&l);
	}

	initLine(&l, (*p).x[i-1], (*p).y[i-1], (*p).x[0], (*p).y[0] ,(*p).r, (*p).g, (*p).b, (*p).a);
	col |= drawLine(&l);
	return col;
}

void fillPolyline(PolyLine* p, int rx, int gx, int bx, int ax) {
	floodFill((*p).xp,(*p).yp, rx,gx,bx,ax, ((*p).r),((*p).g),((*p).b),((*p).a));
}

void drawScreenBorder() {

	// borderwidthd = 200; //ngurangin kanan?
     // borderwidthr = 200; //ngurangin yg bawah
     // borderwidthu = 200; //ngurangin kiri
     // borderwidthl = 200; //ngurangin atas
	
	PolyLine p;
	initPolyline(&p,0,0,255,0);
	addEndPoint(&p, borderwidthu,borderwidthl);
	addEndPoint(&p, vinfo.xres-borderwidthd,borderwidthl);
	addEndPoint(&p, vinfo.xres-borderwidthd,vinfo.yres-borderwidthr);
	addEndPoint(&p, borderwidthu,vinfo.yres-borderwidthr);
	drawPolylineOutline(&p);
  
}
// METODE ANIMASI POLYLINE---------------------------------------------------------------------------------- //
void deletePolyline(PolyLine* p) {

  fillPolyline(p, 0,0,0,0);
	int r = (*p).r;
	int g = (*p).g;
	int b = (*p).b;
	int a = (*p).a;
	(*p).r = 0;
	(*p).g = 0;
	(*p).b = 0;
	(*p).a = 0;
	drawPolylineOutline(p);
	(*p).r = r;
	(*p).g = g;
	(*p).b = b;
	(*p).a = a;
}

// Warning, will remove fill color and only redraw outline
void movePolyline(PolyLine* p, int dx, int dy) {

	deletePolyline(p);
	int tempx;
	int tempy;

	tempx = (*p).xp + dx;
	tempy = (*p).yp + dy;
	(*p).xp = tempx;
	(*p).yp = tempy;

	int i;
	for(i=0; i<(*p).PointCount; i++) {
		tempx = (*p).x[i] + dx;
		tempy = (*p).y[i] + dy;
		(*p).x[i] = tempx;
		(*p).y[i] = tempy;
	}
	drawPolylineOutline(p);
}
void rotatePolyline(PolyLine* p, int xr, int yr, double degrees) {

	deletePolyline(p);
	double cosr = cos((22*degrees)/(180*7));
	double sinr = sin((22*degrees)/(180*7));
	double tempx;
	double tempy;

	tempx = xr + (((*p).xp - xr) * cosr) - (((*p).yp - yr) * sinr);
	tempy = yr + (((*p).xp - xr) * sinr) + (((*p).yp - yr) * cosr);
	(*p).xp = round(tempx);
	(*p).yp = round(tempy);

	int i;
	for(i=0; i<(*p).PointCount; i++) {
		tempx = xr + (((*p).x[i] - xr) * cosr) - (((*p).y[i] - yr) * sinr);
		tempy = yr + (((*p).x[i] - xr) * sinr) + (((*p).y[i] - yr) * cosr);
		(*p).x[i] = round(tempx);
		(*p).y[i] = round(tempy);
	}

	drawPolylineOutline(p);
}
void scalePolyline(PolyLine* p, int xa, int ya, float ratio) {

	deletePolyline(p);
	double tempx;
	double tempy;

	tempx = xa + (((*p).xp - xa) * ratio);
	tempy = ya + (((*p).yp - ya) * ratio);
	(*p).xp = round(tempx);
	(*p).yp = round(tempy);

	int i;
	for(i=0; i<(*p).PointCount; i++) {
		tempx = xa + (((*p).x[i] - xa) * ratio);
		tempy = ya + (((*p).y[i] - ya) * ratio);
		(*p).x[i] = round(tempx);
		(*p).y[i] = round(tempy);
	}

	drawPolylineOutline(p);
}

// PROSEDUR ARRAY OF POLYLINE------------------------------------------------------------------------------- //
typedef struct {

	PolyLine* arr;
	int PolyCount;

} PolyLineArray;

void initPolyLineArray(PolyLineArray* p, int size) {
	(*p).arr = (PolyLine *)malloc(size * sizeof(PolyLine));
	(*p).PolyCount = 0;
}

void addPolyline(PolyLineArray* parr, PolyLine* p) {

	PolyLine* temp = &((*parr).arr[(*parr).PolyCount]);
	(*temp).xp = (*p).xp;
	(*temp).yp = (*p).yp;
	(*temp).PointCount = (*p).PointCount;
	
	(*temp).r = (*p).r;
	(*temp).g = (*p).g;
	(*temp).b = (*p).b;
	(*temp).a = (*p).a;
	
	int i;
	for(i=0; i<(*p).PointCount; i++) {
		(*temp).x[i] = (*p).x[i];
		(*temp).y[i] = (*p).y[i];
	}
	
	(*parr).PolyCount++;

}

void createBangunanArr(PolyLineArray* parr) {
	int br = 255; int bg = 0; int bb = 0; int ba = 0;
	
	initPolyLineArray(parr,100);
	
	PolyLine p;
	
	// Zone 14
	{
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 85, 65);
	addEndPoint(&p, 147, 65);
	addEndPoint(&p, 140, 40);
	addEndPoint(&p, 166, 40);
	addEndPoint(&p, 159, 65);
	addEndPoint(&p, 170, 65);
	addEndPoint(&p, 170, 81);
	addEndPoint(&p, 85, 81);
    setFirePoint(&p, 130, 75);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 87,104, 170,91);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 70, 107);
	addEndPoint(&p, 150, 107);
	addEndPoint(&p, 150, 125);
	addEndPoint(&p, 136, 139);
	addEndPoint(&p, 136, 156);
	addEndPoint(&p, 87, 156);
	addEndPoint(&p, 87, 125);
	addEndPoint(&p, 70, 124);
	setFirePoint(&p, 107, 133);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 61,131, 87,147);
	addPolyline(parr,&p);
}

	// Zone 13
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 81,163, 126,188);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 54,193, 137,239);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 54,239, 98,266);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 54,266, 98,293);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 98,273, 114,287);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 114,266, 132,293);
	addPolyline(parr,&p);
}

	// Zone 12
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 154,162, 217,199);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 154,208, 217,244);
	addPolyline(parr,&p);
}

	// Zone 15
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 197,80, 246,130);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 292,129);
	addEndPoint(&p, 250, 129);
	addEndPoint(&p, 250, 72);
	addEndPoint(&p, 242, 72);
	addEndPoint(&p, 242, 55);
	addEndPoint(&p, 292, 55);
	setFirePoint(&p, 271, 90);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 341, 115);
	addEndPoint(&p, 350, 125);
	addEndPoint(&p, 381, 125);
	addEndPoint(&p, 390, 118);
	addEndPoint(&p, 390, 87);
	addEndPoint(&p, 375, 63);
	addEndPoint(&p, 346, 63);
	addEndPoint(&p, 335, 69);
	addEndPoint(&p, 335, 99);
	addEndPoint(&p, 341, 99);
	setFirePoint(&p, 362, 95);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 398, 58);
	addEndPoint(&p, 455, 58);
	addEndPoint(&p, 455, 123);
	addEndPoint(&p, 446, 123);
	addEndPoint(&p, 446, 134);
	addEndPoint(&p, 398, 134);
	setFirePoint(&p, 426, 98);
	addPolyline(parr,&p);
}

	// Zone 11
	{
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 274, 153);
	addEndPoint(&p, 249, 176);
	addEndPoint(&p, 274, 200);
	addEndPoint(&p, 300, 176);
	setFirePoint(&p, 274, 176);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 274, 208);
	addEndPoint(&p, 249, 231);
	addEndPoint(&p, 274, 255);
	addEndPoint(&p, 300, 231);
	setFirePoint(&p, 274, 231);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 384, 195);
	addEndPoint(&p, 384, 170);
	addEndPoint(&p, 328, 170);
	addEndPoint(&p, 328, 183);
	addEndPoint(&p, 344, 195);
	setFirePoint(&p, 360, 184);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 384, 213);
	addEndPoint(&p, 384, 238);
	addEndPoint(&p, 328, 238);
	addEndPoint(&p, 328, 224);
	addEndPoint(&p, 344, 213);
	setFirePoint(&p, 274, 231);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 392,163, 432,239);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 432,239, 391,273);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 391,273, 354,246);
	addPolyline(parr,&p);
}

	// Zone 10
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 184,299, 303,327);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 184,344, 303,375);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 321,299, 442,329);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 321,346, 442,375);
	addPolyline(parr,&p);
}

	// Zona 9
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 113,362, 174,385);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 90,356, 51,337);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 101, 317);
	addEndPoint(&p, 101, 338);
	addEndPoint(&p, 112, 351);
	addEndPoint(&p, 132, 351);
	addEndPoint(&p, 145, 338);
	addEndPoint(&p, 145, 317);
	addEndPoint(&p, 132, 305);
	addEndPoint(&p, 112, 305);
	setFirePoint(&p, 118, 330);
	addPolyline(parr,&p);
}

	// Zone 16
	{
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 489, 41);
	addEndPoint(&p, 490, 22);
	addEndPoint(&p, 523, 25);
	addEndPoint(&p, 523, 45);
	setFirePoint(&p, 504, 33);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 486, 46);
	addEndPoint(&p, 484, 78);
	addEndPoint(&p, 467, 77);
	addEndPoint(&p, 467, 45);
	setFirePoint(&p, 504, 33);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 492, 45);
	addEndPoint(&p, 516, 46);
	addEndPoint(&p, 515, 82);
	addEndPoint(&p, 491, 79);
	setFirePoint(&p, 504, 64);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 468, 97);
	addEndPoint(&p, 591, 107);
	addEndPoint(&p, 588, 134);
	addEndPoint(&p, 568, 133);
	addEndPoint(&p, 570, 123);
	addEndPoint(&p, 467, 113);
	setFirePoint(&p, 526, 110);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 528, 128);
	addEndPoint(&p, 524, 172);
	addEndPoint(&p, 503, 170);
	addEndPoint(&p, 507, 126);
	setFirePoint(&p, 516, 150);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 475, 176);
	addEndPoint(&p, 568, 184);
	addEndPoint(&p, 571, 144);
	addEndPoint(&p, 583, 145);
	addEndPoint(&p, 577, 198);
	addEndPoint(&p, 474, 189);
	setFirePoint(&p, 525, 187);
	addPolyline(parr,&p);

	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 475, 198);
	addEndPoint(&p, 557, 206);
	addEndPoint(&p, 555, 232);
	addEndPoint(&p, 474, 223);
	setFirePoint(&p, 516, 216);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 473, 236);
	addEndPoint(&p, 565, 245);
	addEndPoint(&p, 563, 268);
	addEndPoint(&p, 471, 258);
	setFirePoint(&p, 517, 252);
	addPolyline(parr,&p);
}
	
	// Zone 4
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 470,283, 550,303);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 491, 311);
	addEndPoint(&p, 550, 311);
	addEndPoint(&p, 550, 336);
	addEndPoint(&p, 540, 336);
	addEndPoint(&p, 540, 330);
	addEndPoint(&p, 515, 330);
	addEndPoint(&p, 515, 338);
	addEndPoint(&p, 491, 338);
	setFirePoint(&p, 504, 33);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 533,340, 556,365);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 533,365, 548,394);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 462,343, 514,388);
	addPolyline(parr,&p);
	}
	
	// Zone 3
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 471,447, 526,471);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 471,484, 526,507);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 467,507, 419,485);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 433, 485);
	addEndPoint(&p, 433, 470);
	addEndPoint(&p, 467, 470);
	addEndPoint(&p, 467, 446);
	addEndPoint(&p, 406, 446);
	addEndPoint(&p, 406, 485);
	setFirePoint(&p, 433, 460);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 427,513, 442,535);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 447,513, 462,535);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 444,537, 462,550);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 406, 436);
	addEndPoint(&p, 523, 436);
	addEndPoint(&p, 523, 409);
	addEndPoint(&p, 423, 409);
	addEndPoint(&p, 423, 404);
	addEndPoint(&p, 406, 404);
	setFirePoint(&p, 465, 421);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 467, 516);
	addEndPoint(&p, 467, 561);
	addEndPoint(&p, 491, 561);
	addEndPoint(&p, 491, 541);
	addEndPoint(&p, 484, 516);
	setFirePoint(&p, 477, 539);
	addPolyline(parr,&p);
	
	}

	// Zone 8
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 71,487, 91,531);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 49,465, 96,479);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 55, 456);
	addEndPoint(&p, 65, 448);
	addEndPoint(&p, 78, 456);
	addEndPoint(&p, 65, 465);
	setFirePoint(&p, 65, 455);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 47,427, 100,448);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 59,405, 100,426);
	addPolyline(parr,&p);
	}

	// Zone 7
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 123,515, 218,531);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 109,492, 218,507);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 194,482, 218,492);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 109,492, 129,484);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 109,469, 151,484);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 157,464, 206,478);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 206,478, 219,410);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 206,459, 158,443);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 206,422, 158,410);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 166,443, 177,422);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 137,402, 153,454);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 114,454, 130,431);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 116,400, 132,430);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 116,426, 107,410);
	addPolyline(parr,&p);
	}

	// Zone 2
	{
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 199, 543);
	addEndPoint(&p, 199, 561);
	addEndPoint(&p, 210, 561);
	addEndPoint(&p, 210, 576);
	addEndPoint(&p, 261, 576);
	addEndPoint(&p, 261, 560);
	addEndPoint(&p, 273, 561);
	addEndPoint(&p, 273, 543);
	addEndPoint(&p, 258, 531);
	addEndPoint(&p, 214, 531);
	setFirePoint(&p, 235, 553);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	addEndPoint(&p, 199+150, 543);
	addEndPoint(&p, 199+150, 561);
	addEndPoint(&p, 210+150, 561);
	addEndPoint(&p, 210+150, 576);
	addEndPoint(&p, 261+150, 576);
	addEndPoint(&p, 261+150, 560);
	addEndPoint(&p, 273+150, 561);
	addEndPoint(&p, 273+150, 543);
	addEndPoint(&p, 258+150, 531);
	addEndPoint(&p, 214+150, 531);
	setFirePoint(&p, 235+150, 553);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 262,587, 298,598);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 327,598, 363,587);
	addPolyline(parr,&p);
	}

	// Zone 6
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 249,414, 303,434);
	addPolyline(parr,&p);
	}
		
	// Zone 5
	{
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 337,413, 393,433);
	addPolyline(parr,&p);
	
	initPolyline(&p, br,bg,bb,ba);
	boxPolyline(&p, 369,500, 394,520);
	addPolyline(parr,&p);
	}
		
	
}

void createJalanArr(PolyLineArray* parr) {
	int br = 128; int bg = 128; int bb = 128; int ba = 0;
	
	initPolyLineArray(parr,100);
	
	PolyLine p;

	// Zone 14
	{
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 39, 155);
		addEndPoint(&p, 40, 85);
		addEndPoint(&p, 45, 75);
		addEndPoint(&p, 52, 70);
		addEndPoint(&p, 56, 65);
		addEndPoint(&p, 62, 60);
		addEndPoint(&p, 70, 55);
		addEndPoint(&p, 80, 50);
		addEndPoint(&p, 142, 50);
		addEndPoint(&p, 145, 60);
		addEndPoint(&p, 80, 60);
		addEndPoint(&p, 70, 65);
		addEndPoint(&p, 62, 70);
		addEndPoint(&p, 56, 75);
		addEndPoint(&p, 52, 80);
		addEndPoint(&p, 50, 85);
		addEndPoint(&p, 49, 155);
		addPolyline(parr,&p);
	}
	// Zone 13
	{
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 39, 155);
		addEndPoint(&p, 38, 240);
		addEndPoint(&p, 42, 250);
		addEndPoint(&p, 42, 295);
		addEndPoint(&p, 48, 295);
		addEndPoint(&p, 48, 250);
		addEndPoint(&p, 48, 240);
		addEndPoint(&p, 49, 155);
		addPolyline(parr,&p);
	}

	// Zone 9
	{
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 42, 295);
		addEndPoint(&p, 38, 400);
		addEndPoint(&p, 44, 400);
		addEndPoint(&p, 48, 295);
		addPolyline(parr,&p);

		//jalan 2
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 46, 392);
		addEndPoint(&p, 520, 392);
		addEndPoint(&p, 530, 397);
		addEndPoint(&p, 530, 403);
		addEndPoint(&p, 520, 398);
		addEndPoint(&p, 46, 398);
		addPolyline(parr,&p);

		// jalan dalam 1
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 98, 392);
		addEndPoint(&p, 98, 360);
		addEndPoint(&p, 104, 360);
		addEndPoint(&p, 104, 392);
		addPolyline(parr,&p);

		// jalan dalam 2
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 176, 392);
		addEndPoint(&p, 176, 350);
		addEndPoint(&p, 166, 335);
		addEndPoint(&p, 166, 300);
		addEndPoint(&p, 44, 300);
		addEndPoint(&p, 50, 296);
		addEndPoint(&p, 170, 296);
		addEndPoint(&p, 170, 335);
		addEndPoint(&p, 180, 350);
		addEndPoint(&p, 180, 392);
		addPolyline(parr,&p);

	}

	// Zone 10
	{
		// jalan dalam 1
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 307, 392);
		addEndPoint(&p, 307, 290);
		addEndPoint(&p, 315, 290);
		addEndPoint(&p, 315, 392);
		addPolyline(parr,&p);

		// jalan dalam 2
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 450, 392);
		addEndPoint(&p, 450, 290);
		addEndPoint(&p, 458, 290);
		addEndPoint(&p, 458, 392);
		addPolyline(parr,&p);
	}

	// Zone 11 dan 12
	{
		//Jalan loop
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 145, 280);
		addEndPoint(&p, 450, 280);
		addEndPoint(&p, 450, 210);
		addEndPoint(&p, 433, 210);
		addEndPoint(&p, 433, 200);
		addEndPoint(&p, 450, 200);
		addEndPoint(&p, 450, 146);
		addEndPoint(&p, 375, 146);
		addEndPoint(&p, 375, 149);
		addEndPoint(&p, 145, 149);
		addEndPoint(&p, 145, 295);
		
		addEndPoint(&p, 139, 295);
		addEndPoint(&p, 139, 143);
		addEndPoint(&p, 375, 143);
		addEndPoint(&p, 458, 143);
		addEndPoint(&p, 458, 210);
		addEndPoint(&p, 458, 290);
		addEndPoint(&p, 145, 290);
		addPolyline(parr,&p);

		//Jalan dalem
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 309, 280);
		addEndPoint(&p, 309, 230);
		addEndPoint(&p, 289, 205);
		addEndPoint(&p, 309, 180);
		addEndPoint(&p, 309, 150);

		addEndPoint(&p, 319, 150);
		addEndPoint(&p, 319, 180);
		addEndPoint(&p, 339, 205);
		addEndPoint(&p, 319, 230);
		addEndPoint(&p, 319, 280);
		addPolyline(parr,&p);

	}

	// Zone 15
	{
		//Jalan dalem
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 300, 142);
		addEndPoint(&p, 300, 50);
		addEndPoint(&p, 235, 50);
		addEndPoint(&p, 235, 75);
		addEndPoint(&p, 190, 75);
		addEndPoint(&p, 190, 140);
		addEndPoint(&p, 184, 140);
		addEndPoint(&p, 184, 42);

		addEndPoint(&p, 464, 42);
		addEndPoint(&p, 464, 87);
		addEndPoint(&p, 560, 87);
		addEndPoint(&p, 560, 95);
		addEndPoint(&p, 464, 95);
		addEndPoint(&p, 464, 145);
		addEndPoint(&p, 460, 145);
		addEndPoint(&p, 460, 50);
		addEndPoint(&p, 330, 50);
		addEndPoint(&p, 330, 142);
		addPolyline(parr,&p);
	}

	//Zone 5 dan 6
	{
		//Jalan dalem
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 305, 398);
		addEndPoint(&p, 305, 577);

		addEndPoint(&p, 330, 577);
		addEndPoint(&p, 330, 398);
		addPolyline(parr,&p);

		//Jalan dalem extended
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 300, 583);
		addEndPoint(&p, 300, 600);

		addEndPoint(&p, 325, 600);
		addEndPoint(&p, 325, 583);
		addPolyline(parr,&p);
	}

	// Zone 8
	{
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 38, 400);
		addEndPoint(&p, 37, 420);
		addEndPoint(&p, 37, 500);
		addEndPoint(&p, 42, 530);
		addEndPoint(&p, 48, 530);
		addEndPoint(&p, 42, 500);
		addEndPoint(&p, 43, 420);
		addEndPoint(&p, 44, 400);
		addPolyline(parr,&p);

		//jalan dalam
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 100, 450);
		addEndPoint(&p, 100, 530);
		addEndPoint(&p, 106, 530);
		addEndPoint(&p, 106, 450);
		addPolyline(parr,&p);

		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 100, 530);
		addEndPoint(&p, 102, 540);
		addEndPoint(&p, 106, 550);
		addEndPoint(&p, 112, 560);
		addEndPoint(&p, 120, 570);
		addEndPoint(&p, 130, 575);
		addEndPoint(&p, 136, 575);
		addEndPoint(&p, 126, 570);
		addEndPoint(&p, 118, 560);
		addEndPoint(&p, 112, 550);
		addEndPoint(&p, 108, 540);
		addEndPoint(&p, 106, 530);
		addPolyline(parr,&p);
	}

	// Zone 2
	{
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 42, 530);
		addEndPoint(&p, 44, 540);
		addEndPoint(&p, 48, 550);
		addEndPoint(&p, 54, 560);
		addEndPoint(&p, 62, 570);
		addEndPoint(&p, 72, 580);
		addEndPoint(&p, 78, 580);
		addEndPoint(&p, 80, 580);
		addEndPoint(&p, 425, 583);
		addEndPoint(&p, 425, 578);
		addEndPoint(&p, 80, 575);
		addEndPoint(&p, 70, 572);
		addEndPoint(&p, 68, 570);
		addEndPoint(&p, 60, 560);
		addEndPoint(&p, 54, 550);
		addEndPoint(&p, 50, 540);
		addEndPoint(&p, 48, 530);
		addPolyline(parr,&p);
	}

	// Zone 2
	{
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 425, 583);
		addEndPoint(&p, 485, 583);
		addEndPoint(&p, 490, 587);
		addEndPoint(&p, 495, 595);
		addEndPoint(&p, 500, 595);
		addEndPoint(&p, 500, 568);
		addEndPoint(&p, 495, 568);
		addEndPoint(&p, 490, 574);
		addEndPoint(&p, 485, 578);
		addEndPoint(&p, 425, 578);
		addPolyline(parr,&p);

		//jalan ke atas
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 495, 568);
		addEndPoint(&p, 495, 510);
		addEndPoint(&p, 530, 510);
		addEndPoint(&p, 530, 400);
		addEndPoint(&p, 535, 400);
		addEndPoint(&p, 535, 515);
		addEndPoint(&p, 500, 515);
		addEndPoint(&p, 500, 568);
		addPolyline(parr,&p);
	}
}

void createPohonArr(PolyLineArray* parr) {
	int br = 0; int bg = 255; int bb = 0; int ba = 0;
	
	initPolyLineArray(parr,100);
	
	PolyLine p;
	
	// zone 2
	{
		/**
		 * Teknik sipil - Albar
		 */
		 
		// left most
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 125, 560);
		addEndPoint(&p, 135, 540);
		addEndPoint(&p, 145, 560);
		addPolyline(parr,&p);
		
		// center
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 150, 560);
		addEndPoint(&p, 160, 540);
		addEndPoint(&p, 170, 560);
		addPolyline(parr,&p);
		
		// right most
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 175, 560);
		addEndPoint(&p, 185, 540);
		addEndPoint(&p, 195, 560);
		addPolyline(parr,&p);
	
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 80, 560);
		addEndPoint(&p, 90, 540);
		addEndPoint(&p, 100, 560);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 48, 510);
		addEndPoint(&p, 55, 490);
		addEndPoint(&p, 65, 510);
		addPolyline(parr,&p);
		
		// center
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 125, 610);
		addEndPoint(&p, 135, 590);
		addEndPoint(&p, 145, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 150, 610);
		addEndPoint(&p, 160, 590);
		addEndPoint(&p, 170, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 175, 610);
		addEndPoint(&p, 185, 590);
		addEndPoint(&p, 195, 610);
		addPolyline(parr,&p);
	
		// right 
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 205, 610);
		addEndPoint(&p, 215, 590);
		addEndPoint(&p, 225, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 230, 610);
		addEndPoint(&p, 240, 590);
		addEndPoint(&p, 250, 610);
		addPolyline(parr,&p);
	
		// left
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 100, 610);
		addEndPoint(&p, 110, 590);
		addEndPoint(&p, 120, 610);
		addPolyline(parr,&p);
	
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 75, 610);
		addEndPoint(&p, 85, 590);
		addEndPoint(&p, 95, 610);
		addPolyline(parr,&p);
	
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 50, 610);
		addEndPoint(&p, 60, 590);
		addEndPoint(&p, 70, 610);
		addPolyline(parr,&p);
			
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 25, 570);
		addEndPoint(&p, 35, 550);
		addEndPoint(&p, 45, 570);
		addPolyline(parr,&p);
	
		// RIGHT side
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 370, 610);
		addEndPoint(&p, 380, 590);
		addEndPoint(&p, 390, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 395, 610);
		addEndPoint(&p, 405, 590);
		addEndPoint(&p, 415, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 420, 610);
		addEndPoint(&p, 430, 590);
		addEndPoint(&p, 440, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 445, 610);
		addEndPoint(&p, 455, 590);
		addEndPoint(&p, 465, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 470, 610);
		addEndPoint(&p, 480, 590);
		addEndPoint(&p, 490, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 510, 610);
		addEndPoint(&p, 520, 590);
		addEndPoint(&p, 530, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 535, 610);
		addEndPoint(&p, 545, 590);
		addEndPoint(&p, 555, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 560, 610);
		addEndPoint(&p, 570, 590);
		addEndPoint(&p, 580, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 585, 610);
		addEndPoint(&p, 595, 590);
		addEndPoint(&p, 605, 610);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 610, 610);
		addEndPoint(&p, 620, 590);
		addEndPoint(&p, 630, 610);
		addPolyline(parr,&p);
		
		// // //
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 510, 580);
		addEndPoint(&p, 520, 560);
		addEndPoint(&p, 530, 580);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 535, 580);
		addEndPoint(&p, 545, 560);
		addEndPoint(&p, 555, 580);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 560, 580);
		addEndPoint(&p, 570, 560);
		addEndPoint(&p, 580, 580);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 585, 580);
		addEndPoint(&p, 595, 560);
		addEndPoint(&p, 605, 580);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 610, 580);
		addEndPoint(&p, 620, 560);
		addEndPoint(&p, 630, 580);
		addPolyline(parr,&p);
		
		// // //
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 510, 550);
		addEndPoint(&p, 520, 530);
		addEndPoint(&p, 530, 550);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 535, 550);
		addEndPoint(&p, 545, 530);
		addEndPoint(&p, 555, 550);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 560, 550);
		addEndPoint(&p, 570, 530);
		addEndPoint(&p, 580, 550);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 585, 550);
		addEndPoint(&p, 595, 530);
		addEndPoint(&p, 605, 550);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 610, 550);
		addEndPoint(&p, 620, 530);
		addEndPoint(&p, 630, 550);
		addPolyline(parr,&p);
		
		// //
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 520);
		addEndPoint(&p, 575, 500);
		addEndPoint(&p, 585, 520);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 540, 520);
		addEndPoint(&p, 550, 500);
		addEndPoint(&p, 560, 520);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 520);
		addEndPoint(&p, 600, 500);
		addEndPoint(&p, 610, 520);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 520);
		addEndPoint(&p, 625, 500);
		addEndPoint(&p, 635, 520);
		addPolyline(parr,&p);
		
		// //
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 540, 490);
		addEndPoint(&p, 550, 470);
		addEndPoint(&p, 560, 490);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 490);
		addEndPoint(&p, 575, 470);
		addEndPoint(&p, 585, 490);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 490);
		addEndPoint(&p, 600, 470);
		addEndPoint(&p, 610, 490);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 490);
		addEndPoint(&p, 625, 470);
		addEndPoint(&p, 635, 490);
		addPolyline(parr,&p);
		
		// //
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 540, 460);
		addEndPoint(&p, 550, 440);
		addEndPoint(&p, 560, 460);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 460);
		addEndPoint(&p, 575, 440);
		addEndPoint(&p, 585, 460);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 460);
		addEndPoint(&p, 600, 440);
		addEndPoint(&p, 610, 460);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 460);
		addEndPoint(&p, 625, 440);
		addEndPoint(&p, 635, 460);
		addPolyline(parr,&p);
		
		// //
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 540, 430);
		addEndPoint(&p, 550, 410);
		addEndPoint(&p, 560, 430);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 430);
		addEndPoint(&p, 575, 410);
		addEndPoint(&p, 585, 430);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 430);
		addEndPoint(&p, 600, 410);
		addEndPoint(&p, 610, 430);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 430);
		addEndPoint(&p, 625, 410);
		addEndPoint(&p, 635, 430);
		addPolyline(parr,&p);
		
		//
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 400);
		addEndPoint(&p, 575, 380);
		addEndPoint(&p, 585, 400);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 400);
		addEndPoint(&p, 600, 380);
		addEndPoint(&p, 610, 400);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 400);
		addEndPoint(&p, 625, 380);
		addEndPoint(&p, 635, 400);
		addPolyline(parr,&p);
		
		//
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 370);
		addEndPoint(&p, 575, 350);
		addEndPoint(&p, 585, 370);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 370);
		addEndPoint(&p, 600, 350);
		addEndPoint(&p, 610, 370);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 370);
		addEndPoint(&p, 625, 350);
		addEndPoint(&p, 635, 370);
		addPolyline(parr,&p);
		
		//
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 340);
		addEndPoint(&p, 575, 320);
		addEndPoint(&p, 585, 340);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 340);
		addEndPoint(&p, 600, 320);
		addEndPoint(&p, 610, 340);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 340);
		addEndPoint(&p, 625, 320);
		addEndPoint(&p, 635, 340);
		addPolyline(parr,&p);
		
		//
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 310);
		addEndPoint(&p, 575, 290);
		addEndPoint(&p, 585, 310);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 310);
		addEndPoint(&p, 600, 290);
		addEndPoint(&p, 610, 310);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 310);
		addEndPoint(&p, 625, 290);
		addEndPoint(&p, 635, 310);
		addPolyline(parr,&p);
		
		//two
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 280);
		addEndPoint(&p, 600, 260);
		addEndPoint(&p, 610, 280);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 280);
		addEndPoint(&p, 625, 260);
		addEndPoint(&p, 635, 280);
		addPolyline(parr,&p);
		
		//two
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 250);
		addEndPoint(&p, 600, 230);
		addEndPoint(&p, 610, 250);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 250);
		addEndPoint(&p, 625, 230);
		addEndPoint(&p, 635, 250);
		addPolyline(parr,&p);
		
		//two
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 220);
		addEndPoint(&p, 600, 200);
		addEndPoint(&p, 610, 220);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 220);
		addEndPoint(&p, 625, 200);
		addEndPoint(&p, 635, 220);
		addPolyline(parr,&p);
	
		//two
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 190);
		addEndPoint(&p, 600, 170);
		addEndPoint(&p, 610, 190);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 190);
		addEndPoint(&p, 625, 170);
		addEndPoint(&p, 635, 190);
		addPolyline(parr,&p);
		
		//two
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 160);
		addEndPoint(&p, 600, 140);
		addEndPoint(&p, 610, 160);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 160);
		addEndPoint(&p, 625, 140);
		addEndPoint(&p, 635, 160);
		addPolyline(parr,&p);
		
		//one
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 130);
		addEndPoint(&p, 625, 110);
		addEndPoint(&p, 635, 130);
		addPolyline(parr,&p);
		
		//two
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 95);
		addEndPoint(&p, 600, 75);
		addEndPoint(&p, 610, 95);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 95);
		addEndPoint(&p, 625, 75);
		addEndPoint(&p, 635, 95);
		addPolyline(parr,&p);
		
		//four
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 540, 65);
		addEndPoint(&p, 550, 45);
		addEndPoint(&p, 560, 65);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 565, 65);
		addEndPoint(&p, 575, 45);
		addEndPoint(&p, 585, 65);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 590, 65);
		addEndPoint(&p, 600, 45);
		addEndPoint(&p, 610, 65);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 615, 65);
		addEndPoint(&p, 625, 45);
		addEndPoint(&p, 635, 65);
		addPolyline(parr,&p);
		
		//four-left
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 230, 470);
		addEndPoint(&p, 240, 450);
		addEndPoint(&p, 250, 470);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 265, 470);
		addEndPoint(&p, 275, 450);
		addEndPoint(&p, 285, 470);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 230, 500);
		addEndPoint(&p, 240, 480);
		addEndPoint(&p, 250, 500);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 265, 510);
		addEndPoint(&p, 275, 490);
		addEndPoint(&p, 285, 510);
		addPolyline(parr,&p);
		
		//four-right
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 350, 480);
		addEndPoint(&p, 360, 460);
		addEndPoint(&p, 370, 480);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 375, 470);
		addEndPoint(&p, 385, 450);
		addEndPoint(&p, 395, 470);
		addPolyline(parr,&p);
		
		// others
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 220, 180);
		addEndPoint(&p, 230, 160);
		addEndPoint(&p, 240, 180);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 230, 205);
		addEndPoint(&p, 240, 185);
		addEndPoint(&p, 250, 205);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 220, 230);
		addEndPoint(&p, 230, 210);
		addEndPoint(&p, 240, 230);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 220, 270);
		addEndPoint(&p, 230, 250);
		addEndPoint(&p, 240, 270);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 190, 270);
		addEndPoint(&p, 200, 250);
		addEndPoint(&p, 210, 270);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 160, 270);
		addEndPoint(&p, 170, 250);
		addEndPoint(&p, 180, 270);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 250, 270);
		addEndPoint(&p, 260, 250);
		addEndPoint(&p, 270, 270);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 280, 270);
		addEndPoint(&p, 290, 250);
		addEndPoint(&p, 300, 270);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 325, 270);
		addEndPoint(&p, 335, 250);
		addEndPoint(&p, 345, 270);
		addPolyline(parr,&p);
		
		// others
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 70, 330);
		addEndPoint(&p, 80, 310);
		addEndPoint(&p, 90, 330);
		addPolyline(parr,&p);
		
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 70, 385);
		addEndPoint(&p, 80, 365);
		addEndPoint(&p, 90, 385);
		addPolyline(parr,&p);
		
		// others
		initPolyline(&p, br,bg,bb,ba);
		addEndPoint(&p, 475, 150);
		addEndPoint(&p, 485, 130);
		addEndPoint(&p, 495, 150);
		addPolyline(parr,&p);

	}
	
}


void drawPolylineArrayOutline(PolyLineArray* parr) {
	int i;
	for(i=0; i<(*parr).PolyCount;i++) {
		drawPolylineOutline(&((*parr).arr[i]));
	}
}

void movePolylineArray(PolyLineArray* parr, int dx, int dy) {
	int i;
	for(i=0; i<(*parr).PolyCount;i++) {
		movePolyline(&((*parr).arr[i]), dx, dy);
	}
}

void scalePolylineArray(PolyLineArray* parr, int ax, int ay, float scale) {
	int i;
	for(i=0; i<(*parr).PolyCount;i++) {
		scalePolyline(&((*parr).arr[i]), ax, ay, scale);
	}
}

PolyLineArray bangunan;
PolyLineArray jalan;
PolyLineArray pohon;
PolyLineArray minibangunan;
PolyLineArray minijalan;
PolyLineArray minipohon;

void drawMiniMapOutline() {
	PolyLine p;
	initPolyline(&p,0,0,255,0);
	addEndPoint(&p, borderwidthu,borderwidthl);
	addEndPoint(&p, vinfo.xres-borderwidthd,borderwidthl);
	addEndPoint(&p, vinfo.xres-borderwidthd,vinfo.yres-borderwidthr);
	addEndPoint(&p, borderwidthu,vinfo.yres-borderwidthr);
	drawPolylineOutline(&p);
}

PolyLine po;

void drawMiniMapPointer() {
	int tempd = borderwidthd;
	int tempu = borderwidthu;
	int tempr = borderwidthr;
	int templ = borderwidthl;
	borderwidthu = vinfo.xres/2 + 5;
	borderwidthd = vinfo.xres - (borderwidthu + 300);
	borderwidthr = vinfo.yres - (borderwidthl + 300);
	

	int xtemp = xmiddle;
	int ytemp = ymiddle;
	xmiddle = borderwidthu + 150;
	ymiddle = borderwidthl + 150;
	initPolyline(&po,0,255,0,0);
	addEndPoint(&po, borderwidthu+1,borderwidthl+1);
	addEndPoint(&po, vinfo.xres-borderwidthd-1,borderwidthl+1);
	addEndPoint(&po, vinfo.xres-borderwidthd-1,vinfo.yres-borderwidthr-1);
	addEndPoint(&po, borderwidthu+1,vinfo.yres-borderwidthr-1);
	setFirePoint(&po, xmiddle, ymiddle);
	drawPolylineOutline(&po);

	borderwidthd = tempd;
	borderwidthr = tempr;
	borderwidthu = tempu;
	borderwidthl = templ;
	xmiddle = xtemp;
	ymiddle = ytemp;
}

void drawMiniMap() {
	// borderwidthd = vinfo.xres - (borderwidthu + 600);
 //     borderwidthr = vinfo.yres - (borderwidthl + 600);
     // borderwidthr = vinfo.xres/2;
     // borderwidthu = vinfo.xres/2;
     // borderwidthl = vinfo.xres/2;

     //borderwidthd = 200; //ngurangin kanan?
     // borderwidthr = 200; //ngurangin yg bawah
     // borderwidthu = 200; //ngurangin kiri
     // borderwidthl = 200; //ngurangin atas

	int tempd = borderwidthd;
	int tempu = borderwidthu;
	int tempr = borderwidthr;
	int templ = borderwidthl;
	borderwidthu = vinfo.xres/2 + 5;
	borderwidthd = vinfo.xres - (borderwidthu + 300);
	borderwidthr = vinfo.yres - (borderwidthl + 300);
	

	int xtemp = xmiddle;
	int ytemp = ymiddle;
	xmiddle = borderwidthu + 150;
	ymiddle = borderwidthl + 150;
	plotPixelRGBA(xmiddle, ymiddle, 255,0,0,0);
	drawMiniMapOutline();
	
	createBangunanArr(&minibangunan);
	createJalanArr(&minijalan);
	createPohonArr(&minipohon);
	
	movePolylineArray(&minibangunan, vinfo.xres/2.4,(vinfo.yres/7*-1));
	movePolylineArray(&minijalan, vinfo.xres/2.4,vinfo.yres/7*-1);
	movePolylineArray(&minipohon, vinfo.xres/2.4,vinfo.yres/7*-1);
	
	scalePolylineArray(&minibangunan, xmiddle, ymiddle, 0.5);
	scalePolylineArray(&minijalan, xmiddle, ymiddle, 0.5);
	scalePolylineArray(&minipohon, xmiddle, ymiddle, 0.5);
	
	drawMiniMapOutline();

	borderwidthd = tempd;
	borderwidthr = tempr;
	borderwidthu = tempu;
	borderwidthl = templ;
	xmiddle = xtemp;
	ymiddle = ytemp;
}


void scalePointer(float scale) {
	int tempd = borderwidthd;
	int tempu = borderwidthu;
	int tempr = borderwidthr;
	int templ = borderwidthl;
	int xtemp = xmiddle;
	int ytemp = ymiddle;
	borderwidthu = 10;
	borderwidthd = 10;
	borderwidthr = 10;
	borderwidthl = 10;
	xmiddle = borderwidthu + 150;
	ymiddle = borderwidthl + 150;
	scalePolyline(&po, po.xp, po.yp, scale);
	borderwidthd = tempd;
	borderwidthr = tempr;
	borderwidthu = tempu;
	borderwidthl = templ;
	xmiddle = xtemp;
	ymiddle = ytemp;
	drawMiniMap();
	drawPolylineOutline(&po);
} 

void movePointer(int dx, int dy) {
	int tempd = borderwidthd;
	int tempu = borderwidthu;
	int tempr = borderwidthr;
	int templ = borderwidthl;
	int xtemp = xmiddle;
	int ytemp = ymiddle;
	borderwidthu = 10;
	borderwidthd = 10;
	borderwidthr = 10;
	borderwidthl = 10;
	xmiddle = borderwidthu + 150;
	ymiddle = borderwidthl + 150;
	movePolyline(&po,dx,dy);
	borderwidthd = tempd;
	borderwidthr = tempr;
	borderwidthu = tempu;
	borderwidthl = templ;
	xmiddle = xtemp;
	ymiddle = ytemp;
	drawMiniMap();
	drawPolylineOutline(&po);
}

// METODE HANDLER THREAD IO--------------------------------------------------------------------------------- //
void *keylistener(void *null) {
    while (1) {
        char X = getch();
        if (X == '\033') {
        	getch();
        	X = getch();

        	if (X == 'C') { // Right arrow
        		movePolylineArray(&bangunan, 10,0);
        		movePolylineArray(&jalan, 10,0);
        		movePolylineArray(&pohon, 10,0);
        		
        		movePointer((1/scale)*-5,0);
        	} else if (X == 'D') { // Left arrow
        		movePolylineArray(&bangunan, -10,0);
        		movePolylineArray(&jalan, -10,0);
        		movePolylineArray(&pohon, -10,0);
        		
        		movePointer((1/scale)*5,0);
        	} else if (X == 'A') { // Up arrow
				movePolylineArray(&bangunan, 0,-10);
				movePolylineArray(&jalan, 0,-10);
				movePolylineArray(&pohon, 0,-10);
				
				movePointer(0,(1/scale)*5);
        	} else if (X == 'B') { // Down arrow
				movePolylineArray(&bangunan, 0,10);
				movePolylineArray(&jalan, 0,10);
				movePolylineArray(&pohon, 0,10);
				
				movePointer(0,(1/scale)*-5);
        	}
            
        } else if ((X == 'i') || (X == 'I')) { // Zoom in
        	scalePolylineArray(&bangunan, xmiddle, ymiddle, 1.1);
        	scalePolylineArray(&jalan, xmiddle, ymiddle, 1.1);
        	scalePolylineArray(&pohon, xmiddle, ymiddle, 1.1);
        	
        	scalePointer(1/1.1);
        	scale *= 1.1;
        } else if ((X == 'o') || (X == 'O')) { // Zoom out
        	scalePolylineArray(&bangunan, xmiddle, ymiddle, 1/1.1);
        	scalePolylineArray(&jalan, xmiddle, ymiddle, 1/1.1);
        	scalePolylineArray(&pohon, xmiddle, ymiddle, 1/1.1);
        	
        	scalePointer(1.1);
        	scale/= 1.1;
        } else if ((X == 'x') || (X == 'X')) {
        	return;
        }
        drawScreenBorder();
    }
}

// PROSEDUR PENGGAMBARAN CIRCLE AND ARC--------------------------------------------------------------------- //
int main(int argc, char *argv[]) {
    
    initScreen();
    clearScreen();

    //drawMiniMap();
    //drawMiniMapPointer();
    
    createBangunanArr(&bangunan);
    drawPolylineArrayOutline(&bangunan);

    createJalanArr(&jalan);
    drawPolylineArrayOutline(&jalan);

    createPohonArr(&pohon);
    drawPolylineArrayOutline(&pohon);

 //    movePolylineArray(&bangunan, vinfo.xres/8,vinfo.yres/10);
	// movePolylineArray(&jalan, vinfo.xres/8,vinfo.yres/10);

	pthread_t listener;
    pthread_create(&listener, NULL, keylistener, NULL);
	pthread_join(listener, NULL);

    terminate();
    return 0;
    
}

