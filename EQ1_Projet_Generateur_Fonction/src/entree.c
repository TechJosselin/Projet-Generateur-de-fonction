#include "entree.h"
static const int8_t transitions[16]={0,1,-1,0,-1,0,0,1,1,0,0,-1,0,-1,1,0};
void quadrature_initialiser(quadrature_t *q,uint8_t ab){q->precedent=ab&3;q->detente=ab&3;q->somme=0;}
int quadrature_traiter(quadrature_t *q,uint8_t ab) {
    ab&=3;if(ab==q->precedent)return 0;
    int p=transitions[(q->precedent<<2)|ab];q->somme=p?q->somme+p:0;q->precedent=ab;
    if(ab!=q->detente)return 0;
    int r=q->somme>=4?1:(q->somme<=-4?-1:0);q->somme=0;return r;
}
void bouton_initialiser(bouton_t *b,bool appuye,uint32_t now) {
    *b=(bouton_t){.brut=appuye,.stable=appuye,.rebond=now,.appui=now};
}
evenement_ui_t bouton_traiter(bouton_t *b,bool appuye,uint32_t now) {
    if(appuye!=b->brut){b->brut=appuye;b->rebond=now;}
    if(b->stable!=appuye && (uint32_t)(now-b->rebond)>=30) {
        b->stable=appuye;
        if(appuye){b->appui=now;b->long_emis=false;}
        else if(!b->long_emis)return UI_CLIC;
    }
    if(b->stable && !b->long_emis && (uint32_t)(now-b->appui)>=800){b->long_emis=true;return UI_APPUI_LONG;}
    return UI_AUCUN;
}
