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

int borderwidth = 25;               // The border width, distance from the actual screenBorder
int xmiddle;
int ymiddle;

void *ioHandler(void *);			// The thread that handle the bullet shooting


// UTILITY PROCEDURE----------------------------------------------------------------------------------------- //

int isOverflow(int _x , int _y) {
	//Cek apakah kooordinat (x,y) sudah melewati batas layar
    int result;
    if ( _x > vinfo.xres-borderwidth || _y > vinfo.yres-borderwidth || _x < borderwidth-1 || _y < borderwidth-1 ) {
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
	 
     xmiddle = vinfo.xres/2;
     ymiddle = vinfo.yres/2;
    
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

int drawLine(Line* l) {
	
	if(isOverflow((*l).x1, (*l).y1) && isOverflow((*l).x2, (*l).y2)) {
		return 0; // Do Nothing if both of the endPoint overflowed
		
	} else if(isOverflow((*l).x1, (*l).y1) || isOverflow((*l).x2, (*l).y2)) {
		// If one of the endPoint overflowed
	
	}
	
	int col = 0;
	
	// Coord. of the next point to be displayed
	int x = (*l).x1;
	int y = (*l).y1;

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
	
	PolyLine p;
	initPolyline(&p,0,0,255,0);
	addEndPoint(&p, borderwidth,borderwidth);
	addEndPoint(&p, borderwidth,vinfo.yres-borderwidth);
	addEndPoint(&p, vinfo.xres-borderwidth,vinfo.yres-borderwidth);
	addEndPoint(&p, vinfo.xres-borderwidth,borderwidth);
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

// METODE HANDLER THREAD IO--------------------------------------------------------------------------------- //

void *ioHandler(void *null) {
	char ch;
	while(1) {

		ch = getchar();
		if (ch == '\n') {

		}

	}

}


// PROSEDUR PENGGAMBARAN CIRCLE AND ARC--------------------------------------------------------------------- //

void drawCircle(double cx, double cy, int radius, int r, int g, int b, int a) {
	inline void plot4points(double cx, double cy, double x, double y) {
		plotPixelRGBA(cx + x, cy + y, r, g, b, a);
	    plotPixelRGBA(cx - x, cy + y, r, g, b, a);
		plotPixelRGBA(cx + x, cy - y, r, g, b, a);
		plotPixelRGBA(cx - x, cy - y, r, g, b, a);
	}

	inline void plot8points(double cx, double cy, double x, double y) {
		plot4points(cx, cy, x, y);
		plot4points(cx, cy, y, x);
	}

	int error = -radius;
	double x = radius;
	double y = 0;

	while (x >= y) {
		plot8points(cx, cy, x, y);

		error += y;
		y++;
		error += y;

		if (error >= 0)
		{
			error += -x;
			x--;
			error += -x;
		}
	}
}

void drawArcUp(double cx, double cy, int radius, int r, int g, int b, int a){
  inline void plot4points(double cx, double cy, double x, double y) {
		plotPixelRGBA(cx + x, cy - y, r, g, b, a);
		plotPixelRGBA(cx - x, cy - y, r, g, b, a);
	}

	inline void plot8points(double cx, double cy, double x, double y) {
		plot4points(cx, cy, x, y);
		plot4points(cx, cy, y, x);
	}

	int error = -radius;
	double x = radius;
	double y = 0;

	while (x >= y) {
		plot8points(cx, cy, x, y);

		error += y;
		y++;
		error += y;

		if (error >= 0) {
			error += -x;
			x--;
			error += -x;
		}
	}

}

void drawArcDown(double cx, double cy, int radius, int r, int g, int b, int a){
  inline void plot4points(double cx, double cy, double x, double y) {
		plotPixelRGBA(cx + x, cy + y, r, g, b, a);
		plotPixelRGBA(cx - x, cy + y, r, g, b, a);
	}

	inline void plot8points(double cx, double cy, double x, double y) {
		plot4points(cx, cy, x, y);
		plot4points(cx, cy, y, x);
	}

	int error = -radius;
	double x = radius;
	double y = 0;

	while (x >= y) {
		plot8points(cx, cy, x, y);

		error += y;
		y++;
		error += y;

		if (error >= 0) {
			error += -x;
			x--;
			error += -x;
		}
	}

}

void *keylistener() {
    while (1) {
        char X = getch();
        if (X == '\033') {
        	getch();
        	X = getch();

        	//Right arrow
        	if (X == 'C') {
        		//Geser ke kanan
        		printf("Right\n");
        	}
        	//Left arrow
        	else if (X == 'D') {
        		//Geser ke kiri
        		printf("Left\n");
        	}
        	//Up arrow
        	else if (X == 'A') {
        		//Geser ke atas
        		printf("Up\n");
        	}
        	//Down arrow
        	else if (X == 'B') {
        		//Geser ke bawah
        		printf("Down\n");
        	}
            
        } else if ((X == 'i') || (X == 'I')) {
        	//Zoom in
        	printf("Zoom in\n");
        } else if ((X == 'o') || (X == 'O')) {
        	//Zoom out
        	printf("Zoom out\n");
        } else if ((X == 'x') || (X == 'X')) {
        	exit(0);
        }
    }
}

// --------------------------------------------------------------------------------------------------------- //

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
	
}

void drawPolylineArrayOutline(PolyLineArray* parr) {
	int i;
	for(i=0; i<(*parr).PolyCount;i++) {
		drawPolylineOutline(&((*parr).arr[i]));
	}
}

int main(int argc, char *argv[]) {
    
    initScreen();
    clearScreen();
    
    PolyLineArray bangunan;
    createBangunanArr(&bangunan);
    drawPolylineArrayOutline(&bangunan);

    terminate();
    return 0;
    
}

