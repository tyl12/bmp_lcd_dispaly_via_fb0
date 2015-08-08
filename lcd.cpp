#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <utils/Log.h>
#include <errno.h>
using namespace android;

typedef struct
{
    char cfType[2];
    long cfSize;
    long cfReserved;
    long cfoffBits;
}__attribute__((packed)) BITMAPFILEHEADER;

typedef struct
{
    char ciSize[4];
    long ciWidth;
    long ciHeight;
    char ciPlanes[2];
    int ciBitCount;
    char ciCompress[4];
    char ciSizeImage[4];
    char ciXPelsPerMeter[4];
    char ciYPelsPerMeter[4];
    char ciClrUsed[4];
    char ciClrImportant[4];
}__attribute__((packed)) BITMAPINFOHEADER;

typedef struct
{
    unsigned short blue;
    unsigned short green;
    unsigned short red;
    unsigned short reserved;
}__attribute__((packed)) PIXEL;

BITMAPFILEHEADER FileHead;
BITMAPINFOHEADER InfoHead;

static char *fbp = 0;
static int xres = 0;
static int yres = 0;
static int bits_per_pixel = 0;

int fbfd = 0;
static void fb_update(struct fb_var_screeninfo *vi)
{
    vi->yoffset = 1;
    ioctl(fbfd, FBIOPUT_VSCREENINFO, vi);
    vi->yoffset = 0;
    ioctl(fbfd, FBIOPUT_VSCREENINFO, vi);
}

int width, height;

static int cursor_bitmpa_format_convert(char *dst,char *src){
    int i ,j ;
    char *psrc = src ;
    char *pdst = dst;
    char *p = psrc;
    int value = 0x00;


    //src order: BGR for 24bit. BGRA for 32bit.
    pdst += (width * height * 4);
    for(i=0;i<height;i++){
        p = psrc + (i+1) * width * 3;
        for(j=0;j<width;j++){
            pdst -= 4;
            p -= 3;
            /*
               pdst[0] = p[0];//B
               pdst[1] = p[1];//G
               pdst[2] = p[2];//R
               */
            pdst[2] = p[0];//B
            pdst[1] = p[1];//G
            pdst[0] = p[2];//R
            //pdst[3] = 0x00;

            value = *((int*)pdst);
            value = pdst[0];
            if(value == 0x00){
                pdst[3] = 0x00;
            }else{
                pdst[3] = 0xff;
            }
        }
    }

    return 0;
}

int show_bmp(const char *path)
{
    FILE *fp;
    int rc;
    int line_x, line_y;
    long int location = 0, BytesPerLine = 0;
    char *bmp_buf = NULL;
    char *bmp_buf_dst = NULL;
    char * buf = NULL;
    int flen = 0;
    int ret = -1;
    int total_length = 0;

    if(path == NULL)
    {
        printf("path Error,return\n");
        return -1;
    }
    printf("path = %s\n", path);
    fp = fopen( path, "rb" );
    if(fp == NULL){
        printf("load > cursor file open failed\n");
        return -1;
    }

    fseek(fp,0,SEEK_SET);
    fseek(fp,0,SEEK_END);
    flen = ftell(fp);

    bmp_buf = (char*)calloc(1,flen - 54);
    if(bmp_buf == NULL){
        printf("load > malloc bmp out of memory!\n");
        return -1;
    }


    fseek(fp,0,SEEK_SET);

    rc = fread(&FileHead, sizeof(BITMAPFILEHEADER),1, fp);
    if ( rc != 1)
    {
        printf("read header error!\n");
        fclose( fp );
        return( -2 );
    }


    if (memcmp(FileHead.cfType, "BM", 2) != 0)
    {
        printf("it's not a BMP file\n");
        fclose( fp );
        return( -3 );
    }
    rc = fread( (char *)&InfoHead, sizeof(BITMAPINFOHEADER),1, fp );
    if ( rc != 1)
    {
        printf("read infoheader error!\n");
        fclose( fp );
        return( -4 );
    }
    width = InfoHead.ciWidth;
    height = InfoHead.ciHeight;
    printf("FileHead.cfSize =%ld byte\n",FileHead.cfSize);
    printf("flen = %d\n", flen);
    printf("width = %d, height = %d\n", width, height);
    total_length = width * height *3;

    printf("total_length = %d\n", total_length);

    fseek(fp, FileHead.cfoffBits, SEEK_SET);
    printf(" FileHead.cfoffBits = %ld\n", FileHead.cfoffBits);
    printf(" InfoHead.ciBitCount = %d\n", InfoHead.ciBitCount);

    buf = bmp_buf;
    while ((ret = fread(buf,1,total_length,fp)) >= 0) {
        if (ret == 0) {
            usleep(100);
            continue;
        }
        printf("ret = %d\n", ret);
        buf = ((char*) buf) + ret;
        total_length = total_length - ret;
        if(total_length == 0)break;
    }

    total_length = width * height * 4;
    printf("total_length = %d\n", total_length);
    bmp_buf_dst = (char*)calloc(1,total_length);
    if(bmp_buf_dst == NULL){
        printf("load > malloc bmp out of memory!\n");
        return -1;
    }

    cursor_bitmpa_format_convert(bmp_buf_dst, bmp_buf);
    memcpy(fbp,bmp_buf_dst,total_length);

    printf("show logo return 0\n");
    return 0;
}

int show_picture(int fd, const char *path)
{

    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    long int screensize = 0;
    struct fb_bitfield red;
    struct fb_bitfield green;
    struct fb_bitfield blue;
    printf("Enter show_logo");

retry1:
    fbfd = fd;//open("/dev/graphics/fb0", O_RDWR);
    printf("fbfd = %d", fbfd);
    if (fbfd == -1)
    {
        printf("Error opening frame buffer errno=%d (%s)\n",
                errno, strerror(errno));
        goto retry1;
    }

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo))
    {
        printf("Error ng fixed information.\n");
        return -1;
    }

    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo))
    {
        printf("Error: reading variable information.\n");
        return -1;
    }

    printf("R:%d,G:%d,B:%d\n", vinfo.red, vinfo.green, vinfo.blue );

    printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel );
    xres = vinfo.xres;
    yres = vinfo.yres;
    bits_per_pixel = vinfo.bits_per_pixel;


    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    printf("screensize=%d byte\n",screensize);


    fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((int)fbp == -1)
    {
        printf("Error: failed to map framebuffer device to memory.\n");
        return -1;
    }

    printf("sizeof file header=%d\n", sizeof(BITMAPFILEHEADER));


    for (int i=0;i<2000;i++){
        show_bmp(path);
        fb_update(&vinfo);
        usleep(1000);
    }


    munmap(fbp, screensize);
    //close(fbfd);
    printf("Exit show_logo\n");
    return 0;
}

int main()
{

    int fd = open("/dev/graphics/fb0", O_RDWR);
    const char path[] = "/data/24b.bmp";
    show_picture(fd,path);
    return 0;
}
