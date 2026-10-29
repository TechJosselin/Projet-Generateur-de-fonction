#include "entree.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"ECHEC: %s\n",#x);exit(1);}}while(0)
int main(void) {
    quadrature_t q;quadrature_initialiser(&q,3);
    const uint8_t a[]={2,0,1,3},b[]={1,0,2,3};int s=0;
    for(unsigned i=0;i<4;i++) {s+=quadrature_traiter(&q,a[i]);}
    CHECK(s==1);
    s=0;
    for(unsigned i=0;i<4;i++) {s+=quadrature_traiter(&q,b[i]);}
    CHECK(s==-1);
    bouton_t p;bouton_initialiser(&p,false,0);
    CHECK(bouton_traiter(&p,true,20)==UI_AUCUN);
    CHECK(bouton_traiter(&p,true,50)==UI_AUCUN);
    CHECK(bouton_traiter(&p,false,100)==UI_AUCUN);
    CHECK(bouton_traiter(&p,false,130)==UI_CLIC);
    CHECK(bouton_traiter(&p,true,300)==UI_AUCUN);
    CHECK(bouton_traiter(&p,true,330)==UI_AUCUN);
    CHECK(bouton_traiter(&p,true,1130)==UI_APPUI_LONG);
    puts("PASS encodeur et bouton (simulateur)");return 0;
}
