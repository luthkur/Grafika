#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>

int fbfd = 0;                       // Filebuffer Filedescriptor
struct fb_var_screeninfo vinfo;     // Struct Variable Screeninfo
struct fb_fix_screeninfo finfo;     // Struct Fixed Screeninfo
long int screensize = 0;            // Ukuran data framebuffer
char *fbp = 0;                      // Framebuffer di memori internal

// UTILITY PROCEDURE----------------------------------------------------------------------------------------- //

int isOverflow(int _x , int _y){
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

void plotPixelRGBA(int _x, int _y, int r, int g, int b, int a) {
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
		if((*(fbp + location) == b)&&(*(fbp + location + 1) == g)&&(*(fbp + location + 2) == r)&&(*(fbp + location + 3) == a)) {
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
    int x;
    int y;
    for (y = 0; y < vinfo.yres - 15 ;y++) {
		for (x = 0; x < vinfo.xres  ; x++) {
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

void drawLine(Line* l) {

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

}

void animateLine(Line* l, int delay, int length) {
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

	// If the absolute gradien is less than 1
	if(dx >= dy) {

		if((*l).x1 < (*l).x2) {

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

				// Add delay and erase trail if necessary
				usleep(delay);
				if(abs(x - (*l).x1) > length) {

					// Erase the next last pixel
					if (!isOverflow(xw,yw)) {
						plotPixelRGBA(xw,yw,255,255,255,0);
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

					// Erase the next last pixel
					if (!isOverflow(xw,yw)) {
						plotPixelRGBA(xw,yw,255,255,255,0);
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
						plotPixelRGBA(xw,yw,255,255,255,0);
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
						plotPixelRGBA(xw,yw,255,255,255,0);
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

void drawPolylineOutline(PolyLine* p) {

	int i; Line l;
	for(i=1; i<(*p).PointCount; i++) {
		initLine(&l, (*p).x[i-1], (*p).y[i-1], (*p).x[i], (*p).y[i] ,(*p).r, (*p).g, (*p).b, (*p).a);
		drawLine(&l);
	}

	initLine(&l, (*p).x[i-1], (*p).y[i-1], (*p).x[0], (*p).y[0] ,(*p).r, (*p).g, (*p).b, (*p).a);
	drawLine(&l);
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

	(*p).xp += dx;
	(*p).yp += dy;

	int i;
	for(i=0; i<(*p).PointCount; i++) {
		(*p).x[i] += dx;
		(*p).y[i] += dy;
	}

	drawPolylineOutline(p);
}


// --------------------------------------------------------------------------------------------------------- //
// void draw_circle(double cx, double cy, int radius)
// {
// 	inline void plot4points(double cx, double cy, double x, double y)
// 	{
// 		drawLine(cx + x, cy + y);
// 	  drawLine(cx - x, cy + y);
// 		drawLine(cx + x, cy - y);
// 		drawLine(cx - x, cy - y);
// 	}
//
// 	inline void plot8points(double cx, double cy, double x, double y)
// 	{
// 		plot4points(cx, cy, x, y);
// 		plot4points(cx, cy, y, x);
// 	}
//
// 	int error = -radius;
// 	double x = radius;
// 	double y = 0;
//
// 	while (x >= y)
// 	{
// 		plot8points(cx, cy, x, y);
//
// 		error += y;
// 		y++;
// 		error += y;
//
// 		if (error >= 0)
// 		{
// 			error += -x;
// 			x--;
// 			error += -x;
// 		}
// 	}
// }




// --------------------------------------------------------------------------------------------------------- //
int main() {

    initScreen();
    clearScreen();

	PolyLine p;
	initPolyline(&p, 255, 255, 255, 0);
	addEndPoint(&p, 100, 100);
	addEndPoint(&p, 100, 200);
	addEndPoint(&p, 200, 200);
	addEndPoint(&p, 200, 100);
	setFirePoint(&p, 150, 150);
	drawPolylineOutline(&p);
	fillPolyline(&p, 100,0,0,0);

	usleep(100000);
	deletePolyline(&p);
	movePolyline(&p, 10,10);
	fillPolyline(&p, 100,0,0,0);

    terminate();

    return 0;
 }
