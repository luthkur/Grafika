#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <math.h>

#define rplane 255
#define gplane 20
#define bplane 0
#define aplane 0

int fbfd = 0;                       // Filebuffer Filedescriptor
struct fb_var_screeninfo vinfo;     // Struct Variable Screeninfo
struct fb_fix_screeninfo finfo;     // Struct Fixed Screeninfo
long int screensize = 0;            // Ukuran data framebuffer
char *fbp = 0;                      // Framebuffer di memori internal

int planeCrash = 0;					// 0 if the plane is still flying
void *startPlane(void *);			// The thread that handle the plane
int x_depan;
int x_belakang;

int dir = 1;						// The current direction of the turret, 0 = LEFT, 1 = MIDDLE, 2 = RIGHT
void *turretHandler(void *);		// The thread that handle the turret and its direction

void *ioHandler(void *);			// The thread that handle the bullet shooting


// UTILITY PROCEDURE----------------------------------------------------------------------------------------- //

int isOverflow(int _x , int _y) {
//Cek apakah kooordinat (x,y) sudah melewati batas layar
    int result;
    if ( _x > vinfo.xres ||  _y > vinfo.yres -20 ) {
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


// STRUKTUR DATA BLOCK CHARACTER---------------------------------------------------------------------------- //

typedef struct {
	int *array;
	size_t used;
	size_t size;
	int x;
	int y;
} Block;

void initArray(Block *a, size_t initialSize) {
  a->array = (int *)malloc(initialSize * sizeof(int));
  a->used = 0;
  a->size = initialSize;
}

void insertArray(Block *a, int element) {
  // a->used is the number of used entries, because a->array[a->used++] updates a->used only *after* the array has been accessed.
  // Therefore a->used can go up to a->size
  if (a->used == a->size) {
    a->size *= 2;
    a->array = (int *)realloc(a->array, a->size * sizeof(int));
  }
  a->array[a->used++] = element;
}

void freeArray(Block *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}

void setCoordinate(Block *a , int _x , int _y){
    a->x = _x;
    a->y = _y;
}


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
	//Mapping framebuffer ke memori internal

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

}

void clearScreen() {
//Mewarnai latar belakang screen dengan warna putih
    int x = 0;
    int y = 0;
    for (y = 0; y < vinfo.yres - 150 ;y++) {
		for (x = 0; x < vinfo.xres ; x++) {
			if (vinfo.bits_per_pixel == 32) {
                plotPixelRGBA(x,y,0,0,0,0);
            } else  { //asumsi mode 16 bit per piksel
				perror("bpp NOT SUPPORTED");
				exit(0);
            }

        }
	}
}


// PROSEDUR PENGGAMBARAN BLOK CHARACTER--------------------------------------------------------------------- //

void initBlock(char* blockfile, Block* blockmem, int _x, int _y) {
	//Membaca file block character kedalam memori, tempatkan di koordinaat (x,y) layar

    FILE * inputFile;
    int pix;
    char see;
    int stop = 0;

    initArray(blockmem, 5);
    inputFile = fopen(blockfile,"r");
    (*blockmem).x = _x;
    (*blockmem).y = _y;

    while (!stop) {
        fscanf(inputFile, "%d", &pix);
        insertArray(blockmem, pix);
        //printf("%d ", pix);
        see = getc(inputFile);
        //printf("isee : %d \n", see );
         if (see == 13) {
            insertArray(blockmem, -1);
            //printf("\n");
         }
         else if( see == EOF) {
            stop = 1;
         }
    }
}

void drawBlock(Block* blockmem) {
//Menggambar block character pada layar

    int i;
    int xCanvas = blockmem->x;
    int yCanvas = blockmem->y;
    int pix;
    for (i =0 ; i < (*blockmem).used ; i++) {
        pix = (*blockmem).array[i];
        if ( pix == -1 ) {
            xCanvas = blockmem->x;
            yCanvas++;
        }
        else {
            if (!isOverflow(xCanvas,yCanvas)){
                plotPixelRGBA(xCanvas,yCanvas,pix,pix,pix,0);
            }
        }
        xCanvas++;
    }

}

void removeBlock(Block* blockmem) {
//Menghapus block character pada layar

    int i;
    int xCanvas = blockmem->x;
    int yCanvas = blockmem->y;
    int pix;
    for (i =0 ; i < (*blockmem).used ; i++) {
        pix = (*blockmem).array[i];
        if ( pix == -1 ) {
            xCanvas =  blockmem->x;
            yCanvas++;
        }
        else {
            if (!isOverflow(xCanvas,yCanvas)){
                plotPixelRGBA(xCanvas,yCanvas,255,255,255,0);
            }
        }
        xCanvas++;
    }
}

void moveUp(Block* blockmem, int px) {
//Animasi gerak ke atas sebesar x pixel

    int j = blockmem -> y;
    int initPx = j;
    for (j ; j > initPx - px ; j = j - 4){
        setCoordinate(blockmem , blockmem->x , j);
        drawBlock(blockmem);
        usleep(5000);
        removeBlock(blockmem);
    }
    drawBlock(blockmem);
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
				if (isPixelColor(x+1,y+1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x+1,y,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x+1,y-1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x,y-1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x-1,y-1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x-1,y,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x-1,y+1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x,y+1,rplane,gplane,bplane,aplane)) col = 1;
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
				if (isPixelColor(x+1,y+1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x+1,y,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x+1,y-1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x,y-1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x-1,y-1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x-1,y,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x-1,y+1,rplane,gplane,bplane,aplane)) col = 1;
				if (isPixelColor(x,y+1,rplane,gplane,bplane,aplane)) col = 1;
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

int animateLine(Line* l, int delay, int length) {
/* Gambar garis "l" di layar mulai dari P1 ke P2 dengan jeda "delay"
 * nanosecond antar pewarnaan pixel; Setelah "length" buah pixel telah
 * digambar, mulai hapus pixel tertua garis sesuai warna BACKGROUND
 * sebelum mewarnai pixel selanjutnya
 */

	// Coord. of the next point to be displayed
	int x = (*l).x1;
	int y = (*l).y1;
	// Coord. of the next point to be deleted
	int xw = (*l).x1;
	int yw = (*l).y1;

	// Calculate initial error factor
	int dx = abs((*l).x2 - (*l).x1);
	int dy = abs((*l).y2 - (*l).y1);
	int p = 0;
	int pw = 0;

	int collision = 0;

	// If the absolute gradien is less than 1
	if(dx >= dy) {

		if((*l).x1 < (*l).x2) {

			// Repeat printing the next pixel until the line is painted
			while(x <= (*l).x2) {

				// Draw the next pixel
				if (!isOverflow(x,y)) {
					if (isPixelColor(x,y,255,255,0,0)) return 1;
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

				// Add delay and erase trail if necessary
				usleep(delay);
				if(abs(x - (*l).x1) > length) {

					// Erase the next last pixel
					if (!isOverflow(xw,yw)) {
						plotPixelRGBA(xw,yw,0,0,0,0);
					}

					// Calculate the next last pixel
					if(pw < 0) {
						pw = pw + 2*dy;
					} else {
						pw = pw + 2*(dy-dx);
						if((*l).y2 - (*l).y1 > 0) yw++;
						else yw--;
					}
					xw++;
				}

			}

		} else {

			// Repeat printing the next pixel until the line is painted
			while(x >= (*l).x2) {

				// Draw the next pixel
				if (!isOverflow(x,y)) {
					if (isPixelColor(x,y,255,255,0,0)) return 1;
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
				x--;

				// Add delay and erase trail if necessary
				usleep(delay);
				if(abs(x - (*l).x1) > length) {

					// Erase the next la0st pixel
					if (!isOverflow(xw,yw)) {
						plotPixelRGBA(xw,yw,0,0,0,0);
					}

					// Calculate the next last pixel
					if(pw < 0) {
						pw = pw + 2*dy;
					} else {
						pw = pw + 2*(dy-dx);
						if((*l).y2 - (*l).y1 > 0) yw++;
						else yw--;
					}
					xw--;
				}

			}

		}



	// If the absolute gradien is more than 1
	} else {

		// Memastikan y1 < y2
		if((*l).y1 < (*l).y2) {

			// Repeat printing the next pixel until the line is painted
			while(y <= (*l).y2) {

				// Draw the next pixel
				if (!isOverflow(x,y)) {
					if (isPixelColor(x,y,255,255,0,0)) return 1;
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

				// Add delay and erase trail if necessary
				usleep(delay);
				if(abs(y - (*l).y1) > length) {

					// Erase the next last pixel
					if (!isOverflow(xw,yw)) {
						plotPixelRGBA(xw,yw,0,0,0,0);
					}

					// Calculate the next last pixel
					if(pw < 0) {
						pw = pw + 2*dx;
					} else {
						pw = pw + 2*(dx-dy);
						if((*l).x2 - (*l).x1 > 0) xw++;
						else xw--;
					}
					yw++;
				}

			}

		} else {

			// Repeat printing the next pixel until the line is painted
			while(y >= (*l).y2) {

				// Draw the next pixel
				if (!isOverflow(x,y)) {
					if (isPixelColor(x,y,255,255,0,0)) return 1;
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
				y--;

				// Add delay and erase trail if necessary
				usleep(delay);
				if(abs(y - (*l).y1) > length) {

					// Erase the next last pixel
					if (!isOverflow(xw,yw)) {
						plotPixelRGBA(xw,yw,0,0,0,0);
					}

					// Calculate the next last pixel
					if(pw < 0) {
						pw = pw + 2*dx;
					} else {
						pw = pw + 2*(dx-dy);
						if((*l).x2 - (*l).x1 > 0) xw++;
						else xw--;
					}
					yw--;
				}

			}

		}

	}

}


// METODE PEWARNAAN UMUM------------------------------------------------------------------------------------ //

void floodFill(int x,int y, int r,int g,int b,int a, int rb,int gb,int bb,int ab) {
// rgba is the color of the fill, rbgbbbab is the color of the border

	if(!isPixelColor(x,y, r,g,b,a)) {
		if(!isPixelColor(x,y, rb,gb,bb,ab)) {
			if((!isOverflow(x,y)) ) {

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

// i = 1 - 0, 1
// i = 2 - 1, 2
// i = 3 - 2, 3
// i = 4 - 3, 0

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

void setFirePoint(PolyLine* p, int x, int y) {
	(*p).xp = x;
	(*p).yp = y;
}

void fillPolyline(PolyLine* p, int rx, int gx, int bx, int ax) {
	floodFill((*p).xp,(*p).yp, rx,gx,bx,ax, ((*p).r),((*p).g),((*p).b),((*p).a));
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


// METODE ANIMASI PESAWAT----------------------------------------------------------------------------------- //

struct PlaneData{
  int xpos;
  int ypos;
  int size;
};

void *startPlane(void *threadarg) {

  struct PlaneData *PlaneThreadData;
  int xpos,ypos,size;
  PlaneThreadData = (struct PlaneData *) threadarg;
  xpos = PlaneThreadData -> xpos;
  ypos = PlaneThreadData -> ypos;
  size = PlaneThreadData -> size;
  PolyLine plane1,plane2,plane3,plane4,plane5;
  int iii = 10;

	x_depan = xpos-size*2;
	x_belakang = xpos+size*6;

	// Initiate the plane, should be offscreen to the right of the screen
	// May be changed according to the screensize
	initPolyline(&plane1, rplane, gplane, bplane, aplane);
	addEndPoint(&plane1, xpos-size*2, ypos-size);
	addEndPoint(&plane1, xpos-size*4, ypos+size);
	addEndPoint(&plane1, xpos+size*6, ypos+size);
	addEndPoint(&plane1, xpos+size*6, ypos-size);
	setFirePoint(&plane1, xpos, ypos);

	initPolyline(&plane2, rplane, gplane, bplane, aplane);
	addEndPoint(&plane2, xpos-size*2, ypos+size);
	addEndPoint(&plane2, xpos+size*2, ypos+size*4);
	addEndPoint(&plane2, xpos+size*2, ypos+size);
	setFirePoint(&plane2, xpos, ypos+size*2);

	initPolyline(&plane3, rplane, gplane, bplane, aplane);
	addEndPoint(&plane3, xpos-size*2, ypos-size);
	addEndPoint(&plane3, xpos+size*2, ypos-size*4);
	addEndPoint(&plane3, xpos+size*2, ypos-size);
	setFirePoint(&plane3, xpos, ypos-size*2);

	initPolyline(&plane4, rplane, gplane, bplane, aplane);
	addEndPoint(&plane4, xpos+size*4, ypos+size);
	addEndPoint(&plane4, xpos+size*6, ypos+size*4);
	addEndPoint(&plane4, xpos+size*6, ypos+size);
	setFirePoint(&plane4, xpos+size*5, ypos+size*2);

	initPolyline(&plane5, rplane, gplane, bplane, aplane);
	addEndPoint(&plane5, xpos+size*4, ypos-size);
	addEndPoint(&plane5, xpos+size*6, ypos-size*4);
	addEndPoint(&plane5, xpos+size*6, ypos-size);
	setFirePoint(&plane5, xpos+size*5, ypos-size*2);

    while (planeCrash==0 && x_belakang>0){
        movePolyline(&plane1, -5, 0);
        if (!planeCrash) fillPolyline(&plane1, 100,200,200,0);
        movePolyline(&plane2, -5, 0);
        if (!planeCrash) fillPolyline(&plane2, 100,200,200,0);
        movePolyline(&plane3, -5, 0);
        if (!planeCrash) fillPolyline(&plane3, 100,200,200,0);
        movePolyline(&plane4, -5, 0);
        if (!planeCrash) fillPolyline(&plane4, 100,200,200,0);
        movePolyline(&plane5, -5, 0);
        if (!planeCrash) fillPolyline(&plane5, 100,200,200,0);
        x_depan -= 5;
        x_belakang -= 5;
        usleep(50005);
    }


    while (iii<1000){
        movePolyline(&plane1, -50, iii);
        fillPolyline(&plane1, 100,200,200,0);
        movePolyline(&plane2, -40, iii);
        fillPolyline(&plane2, 100,200,200,0);
        movePolyline(&plane3, 30, iii);
        fillPolyline(&plane3, 100,200,200,0);
        movePolyline(&plane4, 50, iii);
        fillPolyline(&plane4, 100,200,200,0);
        movePolyline(&plane5, 20, iii);
        fillPolyline(&plane5, 100,200,200,0);
        nanosleep((const struct timespec[]){{0,100000000L}},NULL);
        iii += 50;
    }

	return;
}


// METODE HANDLER THREAD TURRET----------------------------------------------------------------------------- //

void drawTurret(int dir) {

	// DRAW THE TURRET WITH THE APPROPRIATE DIRECTION ACCORDING TO dir PARAMETER
	// 0 = LEFT, 1 = MIDDLE, 2 = RIGHT

	/**
	 * STEPS:
	 * - clear the previous turret
	 * - initiate a new turret with the specified direction
	 * - add the end point (x0, y0 ; x1, y1 ; x2, y2 ; etc) of the turret
	 * - draw the turret
	 */

	PolyLine turretLeftBody, turretMiddleBody, turretRightBody;

	int x_vinfo[15], y_vinfo[15];
	int xnew, ynew;
	int centralPointX, centralPointY;
	centralPointX = vinfo.xres / 2;
	centralPointY = vinfo.yres - 120;
	setFirePoint(&turretLeftBody, centralPointX, centralPointY);
	setFirePoint(&turretMiddleBody, centralPointX, centralPointY);
	setFirePoint(&turretRightBody, centralPointX, centralPointY);


 	// turret's body
 	x_vinfo[0] = (vinfo.xres / 2) - 50;
	y_vinfo[0] = vinfo.yres - 200;
	x_vinfo[1] = (vinfo.xres / 2) + 50;
	y_vinfo[1] = vinfo.yres - 200;
	x_vinfo[2] = (vinfo.xres / 2) + 80;
	y_vinfo[2] = vinfo.yres - 180;
	x_vinfo[3] = (vinfo.xres / 2) + 80;
	y_vinfo[3] = vinfo.yres - 80;
	x_vinfo[4] = (vinfo.xres / 2) + 50;
	y_vinfo[4] = vinfo.yres - 60;
	x_vinfo[5] = (vinfo.xres / 2) - 50;
	y_vinfo[5] = vinfo.yres - 60;
	x_vinfo[6] = (vinfo.xres / 2) - 80;
	y_vinfo[6] = vinfo.yres - 80;
	x_vinfo[7] = (vinfo.xres / 2) - 80;
	y_vinfo[7] = vinfo.yres - 180;

	// turret's shooter
	x_vinfo[8] = (vinfo.xres / 2) - 50;
	y_vinfo[8] = vinfo.yres - 200;
	x_vinfo[9] = (vinfo.xres / 2) - 20;
	y_vinfo[9] = vinfo.yres - 200;
	x_vinfo[10] = (vinfo.xres / 2) - 20;
	y_vinfo[10] = vinfo.yres - 300;
	x_vinfo[11] = (vinfo.xres / 2) + 20;
	y_vinfo[11] = vinfo.yres - 300;
	x_vinfo[12] = (vinfo.xres / 2) + 20;
	y_vinfo[12] = vinfo.yres - 200;


		if (dir == 0) {

			// initiate turret's body
			initPolyline(&turretLeftBody, 255, 255, 255, 0);

			// rotating around point (vinfo.xres / 2, vinfo.yres - 120)

			int ii;
			for (ii = 0; ii < 13; ii++) {

				xnew = cos(100) * (x_vinfo[ii] - centralPointX) - sin(100) * (y_vinfo[ii] - centralPointY) + centralPointX;
				ynew = sin(100) * (x_vinfo[ii] - centralPointX) + cos(100) * (y_vinfo[ii] - centralPointY) + centralPointY;

				addEndPoint(&turretLeftBody, xnew, ynew);

			}

			// draw
			drawPolylineOutline(&turretLeftBody);
			fillPolyline(&turretLeftBody, 255, 0, 0, 0);
			usleep(500000);

			// clear
			deletePolyline(&turretLeftBody);

		} else if (dir == 1) {

			// initiate turret's body
			initPolyline(&turretMiddleBody, 244, 244, 244, 0);
			// end point turret's body
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) - 50, vinfo.yres - 200);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) + 50, vinfo.yres - 200);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) + 80, vinfo.yres - 180);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) + 80, vinfo.yres - 80);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) + 50, vinfo.yres - 60);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) - 50, vinfo.yres - 60);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) - 80, vinfo.yres - 80);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) - 80, vinfo.yres - 180);

			// combined - experiment
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) - 50, vinfo.yres - 200);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) - 20, vinfo.yres - 200);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) - 20, vinfo.yres - 300);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) + 20, vinfo.yres - 300);
			addEndPoint(&turretMiddleBody, (vinfo.xres / 2) + 20, vinfo.yres - 200);

			// draw
			drawPolylineOutline(&turretMiddleBody);
			fillPolyline(&turretMiddleBody, 130, 0, 0, 0);
			usleep(500000);

			// clear
			deletePolyline(&turretMiddleBody);

		} else if (dir == 2) {

			// initiate turret's body
			initPolyline(&turretRightBody, 244, 244, 244, 0);
			// rotating around point (vinfo.xres / 2, vinfo.yres - 120)
			centralPointX = vinfo.xres / 2;
			centralPointY = vinfo.yres - 120;
			int ii;
			for (ii = 0; ii < 13; ii++) {

				xnew = cos(340) * (x_vinfo[ii] - centralPointX) - sin(340) * (y_vinfo[ii] - centralPointY) + centralPointX;
				ynew = sin(340) * (x_vinfo[ii] - centralPointX) + cos(340) * (y_vinfo[ii] - centralPointY) + centralPointY;

				addEndPoint(&turretRightBody, xnew, ynew);

			}

			// draw
			drawPolylineOutline(&turretRightBody);
			fillPolyline(&turretRightBody, 130, 0, 0, 0);
			usleep(500000);

			// clear
			deletePolyline(&turretRightBody);

		}


}

void *turretHandler(void *null) {

	while(planeCrash==0) {

		drawTurret(dir);
		dir++;
		if(dir==3) dir = 0;

	}

	return;
}


// METODE HANDLER THREAD PELURU----------------------------------------------------------------------------- //

int drawBulletUp(int x, int y, int clear){
//NOTICE: IT IS STILL STATIC, THE DIRECTION IS UP
//x : leftmost
//y: topmost

	PolyLine bulletTop, bulletBody, bulletLeftWing, bulletRightWing;

	initPolyline(&bulletTop, 255,0,0,0);
	addEndPoint(&bulletTop, x+14,y+0);
	addEndPoint(&bulletTop, x+9,y+23);
	addEndPoint(&bulletTop, x+19,y+23);
	setFirePoint(&bulletTop, x+14, y+15); //TAKE THE CENTER POINT, ALWAYS

	initPolyline(&bulletBody, 255,255,255,0);
	addEndPoint(&bulletBody, x+19, y+23);
	addEndPoint(&bulletBody, x+9, y+23);
	addEndPoint(&bulletBody, x+9, y+59);
	addEndPoint(&bulletBody, x+19, y+59);
	setFirePoint(&bulletBody, x+14, y+35); //IF THE CENTER POINT CAN'T BE EXACT, ESTIMATE IT

	initPolyline(&bulletLeftWing, 255,255,0,0);
	addEndPoint(&bulletLeftWing, x+9, y+66);
	addEndPoint(&bulletLeftWing, x, y+66);
	addEndPoint(&bulletLeftWing, x+9, y+44);
	setFirePoint(&bulletLeftWing, x+4, y+64); //ALSO THIS

	initPolyline(&bulletRightWing, 255,255,0,0);
	addEndPoint(&bulletRightWing, x+19, y+66);
	addEndPoint(&bulletRightWing, x+28, y+66);
	addEndPoint(&bulletRightWing, x+19, y+44);
	setFirePoint(&bulletRightWing, x+25, y+64); //ALSO THIS

	//DRAW BODY FIRST IN ORDER TO MAKE TOP AND BOTTOM BORDER NOT CHANGED, AND COLORIZE AFTER DRAWING
	int col = 0;
	if(clear) {
		deletePolyline(&bulletRightWing);
		deletePolyline(&bulletLeftWing);
		deletePolyline(&bulletTop);
		deletePolyline(&bulletBody);
	}
	else {
		col |= drawPolylineOutline(&bulletBody);
		fillPolyline(&bulletBody, 255,255,255,0);
		col |= drawPolylineOutline(&bulletTop);
		fillPolyline(&bulletTop, 255,0,0,0);
		col |= drawPolylineOutline(&bulletLeftWing);
		fillPolyline(&bulletLeftWing, 255,255,0,0);
		col |= drawPolylineOutline(&bulletRightWing);
		fillPolyline(&bulletRightWing, 255,255,0,0);
	}

	return col;

}

int drawBulletLeft(int x, int y, int clear){
//NOTICE: IT IS STILL STATIC, THE DIRECTION IS LEFT 30 DEGREES
//x : leftmost
//y: topmost

	PolyLine bulletTop, bulletBody, bulletLeftWing, bulletRightWing;

	initPolyline(&bulletTop, 255,0,0,0);
	addEndPoint(&bulletTop, x,y);
	addEndPoint(&bulletTop, x+16,y+17);
	addEndPoint(&bulletTop, x+8,y+24);
	setFirePoint(&bulletTop, x+8, y+17); //TAKE THE CENTER POINT, ALWAYS

	initPolyline(&bulletBody, 255,255,255,0);
	addEndPoint(&bulletBody, x+16, y+17);
	addEndPoint(&bulletBody, x+34, y+49);
	addEndPoint(&bulletBody, x+26, y+55);
	addEndPoint(&bulletBody, x+8, y+24);
	setFirePoint(&bulletBody, x+26, y+49); //IF THE CENTER POINT CAN'T BE EXACT, ESTIMATE IT

	initPolyline(&bulletLeftWing, 255,255,0,0);
	addEndPoint(&bulletLeftWing, x+18, y+41);
	addEndPoint(&bulletLeftWing, x+30, y+60);
	addEndPoint(&bulletLeftWing, x+21, y+65);
	setFirePoint(&bulletLeftWing, x+23, y+55); //ALSO THIS

	initPolyline(&bulletRightWing, 255,255,0,0);
	addEndPoint(&bulletRightWing, x+38, y+55);
	addEndPoint(&bulletRightWing, x+46, y+50);
	addEndPoint(&bulletRightWing, x+27, y+36);
	setFirePoint(&bulletRightWing, x+38, y+50); //ALSO THIS

	//DRAW BODY FIRST IN ORDER TO MAKE TOP AND BOTTOM BORDER NOT CHANGED, AND COLORIZE AFTER DRAWING

	int col = 0;
	if(clear) {
		initPolyline(&bulletTop, 0,0,0,0);
		initPolyline(&bulletBody, 0,0,0,0);
		initPolyline(&bulletLeftWing, 0,0,0,0);
		initPolyline(&bulletRightWing, 0,0,0,0);
		drawPolylineOutline(&bulletBody);
		fillPolyline(&bulletBody, 0,0,0,0);
		drawPolylineOutline(&bulletTop);
		fillPolyline(&bulletTop, 0,0,0,0);
		drawPolylineOutline(&bulletLeftWing);
		fillPolyline(&bulletLeftWing, 0,0,0,0);
		drawPolylineOutline(&bulletRightWing);
		fillPolyline(&bulletRightWing, 0,0,0,0);
	}
	else {
		col |= drawPolylineOutline(&bulletBody);
		fillPolyline(&bulletBody, 255,255,255,0);
		col |= drawPolylineOutline(&bulletTop);
		fillPolyline(&bulletTop, 255,0,0,0);
		col |= drawPolylineOutline(&bulletLeftWing);
		fillPolyline(&bulletLeftWing, 255,255,0,0);
		col |= drawPolylineOutline(&bulletRightWing);
		fillPolyline(&bulletRightWing, 255,255,0,0);
	}

	return col;

}

int drawBulletRight(int x, int y, int clear){
//NOTICE: IT IS STILL STATIC, THE DIRECTION IS RIGHT 30 DEGREES
//x : leftmost
//y: topmost

	PolyLine bulletTop, bulletBody, bulletLeftWing, bulletRightWing;

	initPolyline(&bulletTop, 255,0,0,0);
	addEndPoint(&bulletTop, x+47,y);
	addEndPoint(&bulletTop, x+30,y+18);
	addEndPoint(&bulletTop, x+38,y+24);
	setFirePoint(&bulletTop, x+35, y+18); //TAKE THE CENTER POINT, ALWAYS

	initPolyline(&bulletBody, 255,255,255,0);
	addEndPoint(&bulletBody, x+38, y+24);
	addEndPoint(&bulletBody, x+30, y+18);
	addEndPoint(&bulletBody, x+12, y+50);
	addEndPoint(&bulletBody, x+20, y+54);
	setFirePoint(&bulletBody, x+26, y+34); //IF THE CENTER POINT CAN'T BE EXACT, ESTIMATE IT

	initPolyline(&bulletLeftWing, 255,255,0,0);
	addEndPoint(&bulletLeftWing, x+0, y+51.5);
	addEndPoint(&bulletLeftWing, x+8, y+56);
	addEndPoint(&bulletLeftWing, x+19, y+37);
	setFirePoint(&bulletLeftWing, x+8, y+50); //ALSO THIS

	initPolyline(&bulletRightWing, 255,255,0,0);
	addEndPoint(&bulletRightWing, x+29, y+41);
	addEndPoint(&bulletRightWing, x+25, y+66);
	addEndPoint(&bulletRightWing, x+17, y+61);
	setFirePoint(&bulletRightWing, x+22, y+58); //ALSO THIS

	//DRAW BODY FIRST IN ORDER TO MAKE TOP AND BOTTOM BORDER NOT CHANGED, AND COLORIZE AFTER DRAWING
	int col = 0;
	if(clear) {
		initPolyline(&bulletTop, 0,0,0,0);
		initPolyline(&bulletBody, 0,0,0,0);
		initPolyline(&bulletLeftWing, 0,0,0,0);
		initPolyline(&bulletRightWing, 0,0,0,0);
		drawPolylineOutline(&bulletBody);
		fillPolyline(&bulletBody, 0,0,0,0);
		drawPolylineOutline(&bulletTop);
		fillPolyline(&bulletTop, 0,0,0,0);
		drawPolylineOutline(&bulletLeftWing);
		fillPolyline(&bulletLeftWing, 0,0,0,0);
		drawPolylineOutline(&bulletRightWing);
		fillPolyline(&bulletRightWing, 0,0,0,0);
	}	else {
		col |= drawPolylineOutline(&bulletBody);
		fillPolyline(&bulletBody, 255,255,255,0);
		col |= drawPolylineOutline(&bulletTop);
		fillPolyline(&bulletTop, 255,0,0,0);
		col |= drawPolylineOutline(&bulletLeftWing);
		fillPolyline(&bulletLeftWing, 255,255,0,0);
		col |= drawPolylineOutline(&bulletRightWing);
		fillPolyline(&bulletRightWing, 255,255,0,0);
	}

	return col;

}

void animateBullet(int dir) {

	// DRAW THE BULLET SHOOTING TO A DIRECTION FROM THE TURRET
	// 0 = LEFT, 1 = MIDDLE, 2 = RIGHT
	// TERMINATE ONCE THE BULLET DISAPEAR OR HIT THE PLANE

	// HIT COLLISION DETECTION ALSO HAPPEN HERE

	int xbase = vinfo.xres/2 - 15, ybase = vinfo.yres -380;

	/*Line a;
	int x;
	if (dir == 0) x = xbase - 0.5*xbase;
	else if (dir == 1) x = xbase;
	else x = xbase + 0.5*xbase;
	initLine(&a, xbase, ybase, x, 0, 255, 255, 255, 0);
	int col = animateLine(&a, 100, 100);
	if (col) printf("COLLISION\n");
	initLine(&a, xbase, ybase, x, 0, 0, 0, 0, 0);
	animateLine(&a, 0, 0);*/

	int col = 0;
	int y;

	if (dir == 1) {
		for (y=ybase;y>-100;y--) {
			col = drawBulletUp(xbase,y,0);
			usleep(2000);
			drawBulletUp(xbase,y,1);
			if (col) {
				planeCrash++;
				return;
			}
		}
	} else if (dir == 0) {
		xbase -= 150;

		// Coord. of the next point to be displayed
		int x = xbase;
		int y = ybase;

		// Calculate initial error factor
		int dx = abs(xbase);
		int dy = abs(ybase+50);
		int p = 0;

		// If the absolute gradien is less than 1
		if(dx >= dy) {

			// Repeat printing the next pixel until the line is painted
			while(y >= -50) {

				// Draw the next pixel
				col = drawBulletLeft(x,y,0);
				usleep(2000);
				drawBulletLeft(x,y,1);
				if (col) {
					planeCrash++;
					return;
				}


				// Calculate the next pixel
				if(p < 0) {
					p = p + 2*dy;
				} else {
					p = p + 2*(dy-dx);

					y--;
				}
				x--;

			}

		// If the absolute gradien is more than 1
		} else {

			// Repeat printing the next pixel until the line is painted
			while(y >= -50) {

				// Draw the next pixel
				col = drawBulletLeft(xbase,y,0);
				usleep(2000);
				drawBulletLeft(xbase,y,1);
				if (col) {
					planeCrash++;
					return;
				}

				// Calculate the next pixel
				if(p < 0) {
					p = p + 2*dx;
				} else {
					p = p + 2*(dx-dy);
					x--;
				}
				y--;
			}

		}
	} else if (dir == 2) {
		xbase += 150;

		// Coord. of the next point to be displayed
		int x = xbase;
		int y = ybase;

		// Calculate initial error factor
		int dx = abs(xbase-vinfo.xres);
		int dy = abs(ybase+50);
		int p = 0;

		// If the absolute gradien is less than 1
		if(dx >= dy) {

			// Repeat printing the next pixel until the line is painted
			while(y >= -50) {

				// Draw the next pixel
				col = drawBulletRight(x,y,0);
				usleep(2000);
				drawBulletRight(x,y,1);
				if (col) {
					planeCrash++;
					return;
				}


				// Calculate the next pixel
				if(p < 0) {
					p = p + 2*dy;
				} else {
					p = p + 2*(dy-dx);

					y--;
				}
				x++;

			}

		// If the absolute gradien is more than 1
		} else {

			// Repeat printing the next pixel until the line is painted
			while(y >= -50) {

				// Draw the next pixel
				col = drawBulletRight(xbase,y,0);
				usleep(2000);
				drawBulletRight(xbase,y,1);
				if (col) {
					planeCrash++;
					return;
				}

				// Calculate the next pixel
				if(p < 0) {
					p = p + 2*dx;
				} else {
					p = p + 2*(dx-dy);
					x++;
				}
				y--;
			}

		}
	}

	return;
}

void *ioHandler(void *null) {

	char ch;
	while(1) {

		ch = getchar();
		if (ch == '\n') {
			animateBullet(dir);
			if(planeCrash) return;

		}

	}

}


// --------------------------------------------------------------------------------------------------------- //

void drawCircle(double cx, double cy, int radius, int r, int g, int b, int a) {
	inline void plot4points(double cx, double cy, double x, double y)
	{
		plotPixelRGBA(cx + x, cy + y, r, g, b, a);
	  plotPixelRGBA(cx - x, cy + y, r, g, b, a);
		plotPixelRGBA(cx + x, cy - y, r, g, b, a);
		plotPixelRGBA(cx - x, cy - y, r, g, b, a);
	}

	inline void plot8points(double cx, double cy, double x, double y)
	{
		plot4points(cx, cy, x, y);
		plot4points(cx, cy, y, x);
	}

	int error = -radius;
	double x = radius;
	double y = 0;

	while (x >= y)
	{
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

//---------------///
void drawArcUp(double cx, double cy, int radius, int r, int g, int b, int a){
  inline void plot4points(double cx, double cy, double x, double y)
	{
		//plotPixelRGBA(cx + x, cy + y, 255, 255, 255, 0);
	  //plotPixelRGBA(cx - x, cy + y, 255, 255, 255, 0);
		plotPixelRGBA(cx + x, cy - y, r, g, b, a);
		plotPixelRGBA(cx - x, cy - y, r, g, b, a);
	}

	inline void plot8points(double cx, double cy, double x, double y)
	{
		plot4points(cx, cy, x, y);
		plot4points(cx, cy, y, x);
	}

	int error = -radius;
	double x = radius;
	double y = 0;

	while (x >= y)
	{
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
void drawArcDown(double cx, double cy, int radius, int r, int g, int b, int a){
  inline void plot4points(double cx, double cy, double x, double y)
	{
		plotPixelRGBA(cx + x, cy + y, r, g, b, a);
		plotPixelRGBA(cx - x, cy + y, r, g, b, a);
	}

	inline void plot8points(double cx, double cy, double x, double y)
	{
		plot4points(cx, cy, x, y);
		plot4points(cx, cy, y, x);
	}

	int error = -radius;
	double x = radius;
	double y = 0;

	while (x >= y)
	{
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

void drawParachute(int x, int y){
  drawArcUp(x+45, y+47, 45, 255,0,0,0);
  PolyLine cover;
  initPolyline(&cover, 255, 0, 0, 0);
  addEndPoint(&cover, x, y+47);
  addEndPoint(&cover, x+90, y+47);
  drawPolylineOutline(&cover);

  drawCircle(x+13.3, y+47,10, 255,255,0,0);
  floodFill(x+13.3, y+47,255,255,0,0,255,255,0,0);
  drawCircle(x+13.3, y+47,10, 255,0,0,0);
  floodFill(x+13.3, y+47,0,0,0,0,255,0,0,0);

  drawCircle(x+34.3, y+47,10, 255,255,0,0);
  floodFill(x+34.3, y+47,255,255,0,0,255,255,0,0);
  drawCircle(x+34.3, y+47,10, 255,0,0,0);
  floodFill(x+34.3, y+47,0,0,0,0,255,0,0,0);

  drawCircle(x+56.3, y+47,10, 255,255,0,0);
  floodFill(x+56.3, y+47,255,255,0,0,255,255,0,0);
  drawCircle(x+56.3, y+47,10, 255,0,0,0);
  floodFill(x+56.3, y+47,0,0,0,0,255,0,0,0);

  drawCircle(x+77.3, y+47,10, 255,255,0,0);
  floodFill(x+77.3, y+47,255,255,0,0,255,255,0,0);
  drawCircle(x+77.3, y+47,10, 255,0,0,0);
  floodFill(x+77.3, y+47,0,0,0,0,255,0,0,0);


  drawArcDown(x+13.3, y+47,10, 0,0,0,0);
  drawArcDown(x+34.3, y+47,10, 0,0,0,0);
  drawArcDown(x+56.3, y+47,10, 0,0,0,0);
  drawArcDown(x+77.3, y+47,10, 0,0,0,0);

  floodFill(x+45, y+20, 0,255,0,0,255,0,0,0);

  PolyLine p1,p2,p3,p4,p5;
  initPolyline(&p1,255,255,255,0);
  addEndPoint(&p1, x+2, y+47);
  addEndPoint(&p1, x+45, y+100);
  drawPolylineOutline(&p1);

  initPolyline(&p2,255,255,255,0);
  addEndPoint(&p2, x+23.5, y+47);
  addEndPoint(&p2, x+45, y+100);
  drawPolylineOutline(&p2);

  initPolyline(&p3,255,255,255,0);
  addEndPoint(&p3, x+45, y+47);
  addEndPoint(&p3, x+45, y+100);
  drawPolylineOutline(&p3);

  initPolyline(&p4,255,255,255,0);
  addEndPoint(&p4, x+66.5, y+47);
  addEndPoint(&p4, x+45, y+100);
  drawPolylineOutline(&p4);

  initPolyline(&p5,255,255,255,0);
  addEndPoint(&p5, x+88, y+47);
  addEndPoint(&p5, x+45, y+100);
  drawPolylineOutline(&p5);
}


//------//
void drawPropeller(int x, int y, int scale){
  PolyLine up, down;
  initPolyline(&up, 255,255,0,0);
  addEndPoint(&up, x+(5*scale), y);
  addEndPoint(&up, x+(7*scale), y+(5*scale));
  addEndPoint(&up, x+(scale*7), y+(scale*28));
  addEndPoint(&up, x+(scale*3), y+(scale*28));
  addEndPoint(&up, x+(scale*3), y+(scale*5));
  setFirePoint(&up, x+(scale*5), y+(scale*5));

  initPolyline(&down, 255,255,0,0);
  addEndPoint(&down, x+(5*scale), y+(scale*56));
  addEndPoint(&down, x+(7*scale), y+(scale*51));
  addEndPoint(&down, x+(7*scale), y+(scale*28));
  addEndPoint(&down, x+(3*scale), y+(scale*28));
  addEndPoint(&down, x+(3*scale), y+(scale*51));
  setFirePoint(&down, x+(scale*5), y+(scale*50));


  drawCircle(x+(5*scale),y+(28*scale),5*scale,255,255,0,0);
  floodFill(x+(5*scale), y+(28*scale),255,255,0,0,255,255,0,0);

  drawPolylineOutline(&up);
  fillPolyline(&up, 255,255,0,0);

  drawPolylineOutline(&down);
  fillPolyline(&down, 255,255,0,0);

}


//------//

int main(int argc, char *argv[]) {
/*
	struct PlaneData AddPlaneData;
	AddPlaneData.xpos = atoi(argv[1]);
	AddPlaneData.ypos = atoi(argv[2]);
	AddPlaneData.size = atoi(argv[3]);
	printf("xposition : %d\n", AddPlaneData.xpos);
	printf("yposition : %d\n", AddPlaneData.ypos);
	printf("size constan : %d\n", AddPlaneData.size);

    initScreen();
    clearScreen();

	pthread_t planeThread, turretThread, ioThread;
	pthread_create(&planeThread,NULL,startPlane,(void *) &AddPlaneData);
	pthread_create(&turretThread,NULL,turretHandler,NULL);
	pthread_create(&ioThread,NULL,ioHandler,NULL);

	pthread_join(turretThread, NULL);
	pthread_join(ioThread, NULL);
	pthread_join(planeThread, NULL);
	terminate();
  */
    initScreen();
    clearScreen();

	// PolyLine p;
	// initPolyline(&p, 255,0,0,0);
	// addEndPoint(&p, 200,150);
	// addEndPoint(&p, 200,200);
	// addEndPoint(&p, 150,200);
	// addEndPoint(&p, 150,150);
	//
	// setFirePoint(&p, 175, 175);
	// drawPolylineOutline(&p);
	// fillPolyline(&p, 0,255,0,0);
	//
	// int i;
	// for(i=0; i<50; i++) {
	// 	usleep(100000);
	// 	rotatePolyline(&p,p.xp,p.yp,10);
	// 	movePolyline(&p,10,0);
	// 	fillPolyline(&p, 0,255,0,0);
	// }
  //drawParachute(200,200);
  drawPropeller(200,200,4);
	terminate();
    return 0;
 }
