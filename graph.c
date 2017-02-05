#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <math.h>

int fbfd = 0;                       // Filebuffer Filedescriptor
struct fb_var_screeninfo vinfo;     // Struct Variable Screeninfo
struct fb_fix_screeninfo finfo;     // Struct Fixed Screeninfo
long int screensize = 0;            // Ukuran data framebuffer
char *fbp = 0;                      // Framebuffer di memori internal

int planeCrash = 0;					// 0 if the plane is still flying
void *startPlane(void *);			// The thread that handle the plane
	
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
    printf("wtf\n");
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


// METODE ANIMASI PESAWAT----------------------------------------------------------------------------------- //

void *startPlane(void *null) {

	PolyLine plane1,plane2,plane3,plane4,plane5;
	while(1) {
		
		// Initiate the plane, should be offscreen to the right of the screen
		// May be changed according to the screensize
		initPolyline(&plane1, 255, 255, 0, 0);
		addEndPoint(&plane1, 500, 200);
		addEndPoint(&plane1, 400, 300);
		addEndPoint(&plane1, 600, 300);
		addEndPoint(&plane1, 900, 300);
		addEndPoint(&plane1, 900, 100);
		addEndPoint(&plane1, 700, 200);
		addEndPoint(&plane1, 600, 200);
		
		initPolyline(&plane2, 255, 255, 0, 0);
		addEndPoint(&plane2, 500, 300);
		addEndPoint(&plane2, 700, 450);
		addEndPoint(&plane2, 700, 300);
		initPolyline(&plane3, 255, 255, 0, 0);
		addEndPoint(&plane3, 500, 200);
		addEndPoint(&plane3, 700, 50);
		addEndPoint(&plane3, 700, 200);
		initPolyline(&plane4, 255, 255, 0, 0);
		addEndPoint(&plane4, 800, 300);
		addEndPoint(&plane4, 900, 400);
		addEndPoint(&plane4, 900, 300);
		initPolyline(&plane5, 255, 255, 0, 0);
		addEndPoint(&plane5, 800, 200);
		addEndPoint(&plane5, 900, 100);
		addEndPoint(&plane5, 900, 200);
		
		drawPolylineOutline(&plane1);
		
		drawPolylineOutline(&plane2);
		drawPolylineOutline(&plane3);
		drawPolylineOutline(&plane4);
		drawPolylineOutline(&plane5);
		
		// NEED TO COLOR THE PLANE HERE
		
		// ITERATE MOVE PLANE LEFT AND SLEEP, IF PLANE NO LONGER VISIBLE, 
		// RESET THE PLANE (EXIT LOOP)
		// DURING EACH LOOP CHECK THE VALUE OF planeCrash VARIABLE
		// IF ==1, EXIT THE LOOP INTO THE CRASH ANIMATION
		
	}
	
	// ANIMATE THE CRASH FOR EACH POLYLINE 1-5 HERE
	
	return;
}


// METODE HANDLER THREAD TURRET----------------------------------------------------------------------------- //

void drawTurret(int dir) {
	
	// DRAW THE TURRET WITH THE APPROPRIATE DIRECTION ACCORDING TO dir PARAMETER
	// 0 = LEFT, 1 = MIDDLE, 2 = RIGHT
	
	// THE END POINT COORDINATE DEPENDS ON YOUR SCREEN'S WIDTH AND HEIGHT
	
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
			initPolyline(&turretMiddleBody, 255, 255, 255, 0);
			
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
			
			usleep(500000);
			
			// clear
			deletePolyline(&turretMiddleBody);
	
		} else if (dir == 2) {
		
			// initiate turret's body
			initPolyline(&turretRightBody, 255, 255, 255, 0);
			
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
		
			usleep(500000);
			
			// clear
			deletePolyline(&turretRightBody);
	
		}
		
		
}

void *turretHandler(void *null) {
	
	printf("turretHandler %d %d\n", planeCrash, dir);
	
	while(planeCrash==0) {
		
		drawTurret(dir);
		dir++;
		printf("%d\n", dir);
		if(dir==3) dir = 0;
    
	}
	
	return;
}


// METODE HANDLER THREAD PELURU----------------------------------------------------------------------------- //

void animateBullet(int dir) {
	
	// DRAW THE BULLET SHOOTING TO A DIRECTION FROM THE TURRET
	// 0 = LEFT, 1 = MIDDLE, 2 = RIGHT
	// TERMINATE ONCE THE BULLET DISAPEAR OR HIT THE PLANE
	
	// HIT COLLISION DETECTION ALSO HAPPEN HERE

	int xbase = vinfo.xres/2, ybase = vinfo.yres -120;

	Line a;
	int x;
	if (dir == 0) x = xbase - 0.5*xbase;
	else if (dir == 1) x = xbase;
	else x = xbase + 0.5*xbase;
	initLine(&a, xbase, ybase, x, 0, 255, 255, 255, 0);
	int col = animateLine(&a, 100, 100);
	if (col) printf("COLLISION\n");
	initLine(&a, xbase, ybase, x, 0, 0, 0, 0, 0);
	animateLine(&a, 0, 0);

	
	return;
}

void *ioHandler(void *null) {
	
	char ch;
	while(1) {
		
		ch = getchar();
		if (ch == '\n') {
			
			animateBullet(dir);
			if(planeCrash==10) return;
			
		}
		
	}
	
}


// --------------------------------------------------------------------------------------------------------- //
void draw_circle(double cx, double cy, int radius)
{
	inline void plot4points(double cx, double cy, double x, double y)
	{
		plotPixelRGBA(cx + x, cy + y, 255, 255, 255, 0);
	  plotPixelRGBA(cx - x, cy + y, 255, 255, 255, 0);
		plotPixelRGBA(cx + x, cy - y, 255, 255, 255, 0);
		plotPixelRGBA(cx - x, cy - y, 255, 255, 255, 0);
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



// --------------------------------------------------------------------------------------------------------- //

int main() {

    initScreen();
    clearScreen();
       
	pthread_t planeThread, turretThread, ioThread;
    pthread_create(&planeThread,NULL,startPlane,NULL);
	pthread_create(&turretThread,NULL,turretHandler,NULL);
	pthread_create(&ioThread,NULL,ioHandler,NULL);

	draw_circle(500, 500, 20);
	floodFill(500, 500, 255,0,0,0,255,255,255,0);
	
	pthread_join(turretThread, NULL);
	pthread_join(ioThread, NULL);
    pthread_join(planeThread, NULL);
    
    terminate();
    return 0;
 }
