#include "analogique.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    FILE *f=fopen(argv[1],"rb");if(!f)return 2;
    uint8_t b[512],out[512];size_t n=fread(b,1,sizeof(b),f);int extra=fgetc(f);fclose(f);
    calibration_t p;
    if(n!=512 || extra!=EOF || calibration_decoder(b,&p)!=GF_OK ||
       calibration_encoder(&p,out)!=GF_OK || memcmp(b,out,512))return 1;
    puts("PASS codec croise : profil produit par Python, decode/reencode identique en C.");return 0;
}
