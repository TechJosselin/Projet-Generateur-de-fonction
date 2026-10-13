        if(confirmation==CONFIRMER_SUPPRESSION)aller(PAGE_DETAIL);
        else if(confirmation==CONFIRMER_VOUT_ON)aller(PAGE_SORTIE);
        else aller(PAGE_MEMOIRE);
        break;
    case PAGE_MESSAGE:aller(apres_message);break;
    default:break;
    }
}
/* Rend 5 lignes de liste scrollables, pour affichage 128x64. */
static void liste_scrollee(const char *titre_s,int curseur,int total,
                           void (*fabriquer)(int,char *,size_t)){
    titre(titre_s);
    int premiere=borne(curseur-2,0,total>5?total-5:0);
    for(int i=0;i<5;i++){
        int index=premiere+i;
        if(index>=total)break;
        char ligne[52];fabriquer(index,ligne,sizeof(ligne));
        oled_ligne(2+i,ligne,index==curseur);
    }
    pied("CLIC:OK LONG:RETOUR");
}
static void ligne_accueil(int i,char *dst,size_t n){snprintf(dst,n,"%s",accueil[i]);}
static void ligne_signal(int i,char *dst,size_t n){
    const signal_config_t *s=signaux_lire((size_t)i);
    if(!s){*dst=0;return;}
    char a[24],f[24];
    signaux_formater_tension(a,sizeof(a),s->amplitude_mvpp,false);
    signaux_formater_frequence(f,sizeof(f),s->frequence_hz);
    snprintf(dst,n,"%02d %s %s %s",i+1,signaux_type_court(s->forme),a,f);
}
static void ligne_detail(int i,char *dst,size_t n){
    const signal_config_t *s=actuel();
    if(!s){*dst=0;return;}
    char a[24];
    switch(i){
    case 0:snprintf(dst,n,"Choisir signal");break;
    case 1:snprintf(dst,n,"Type: %s",signaux_type_long(s->forme));break;
    case 2:
        signaux_formater_tension(a,sizeof(a),s->amplitude_mvpp,false);
        snprintf(dst,n,"Amplitude: %spp",a);break;
    case 3:
        signaux_formater_tension(a,sizeof(a),s->offset_mv,true);
        snprintf(dst,n,"Offset: %s",a);break;
    case 4:
        signaux_formater_frequence(a,sizeof(a),s->frequence_hz);
        snprintf(dst,n,"Freq.: %s",a);break;
    default:snprintf(dst,n,"%s",detail_fin[i-5]);break;
    }
}
static void ligne_sortie(int i,char *dst,size_t n){snprintf(dst,n,"%s",sortie_options[i]);}
static void ligne_memoire(int i,char *dst,size_t n){snprintf(dst,n,"%s",memoire_options[i]);}
static void ligne_usb(int i,char *dst,size_t n){snprintf(dst,n,"%s",usb_options[i]);}
static void ligne_reglages(int i,char *dst,size_t n){snprintf(dst,n,"%s",reglages_options[i]);}

static void afficher_accueil(void){
    liste_scrollee("GENERATEUR / MENU",c_accueil,NB_ACCUEIL,ligne_accueil);
    char s[28];int index=integration_signal_selectionne();
    const char *vout=integration_sortie_reelle_active()?"ON":"OFF";
    if(index<0)snprintf(s,sizeof(s),"SIG: RIEN / VOUT %s",vout);
    else snprintf(s,sizeof(s),"SIG: %02d / VOUT %s",index+1,vout);
    oled_ligne(1,s,false);
}
static void afficher_liste(void){
    liste_scrollee("SIGNAUX / LISTE",c_signal,(int)signaux_nombre(),ligne_signal);
    char s[28];snprintf(s,sizeof(s),"%02d / %02d  MAX 100",c_signal+1,(int)signaux_nombre());
    oled_ligne(1,s,false);
}
static void afficher_detail(void){
    liste_scrollee("SIGNAL / OPTIONS",c_detail,NB_DETAIL,ligne_detail);
    char s[32];
    snprintf(s,sizeof(s),"SIG %02d %s%s",c_signal+1,
       signaux_modifie((size_t)c_signal)?"*RAM ":"",
       integration_signal_selectionne()==c_signal?"SELECT.":"");
    oled_ligne(1,s,false);
}
static uint32_t pas_pour(signal_champ_t champ){
    if(champ==CHAMP_AMPLITUDE)return pas_volt[pas_ampl_idx];
    if(champ==CHAMP_OFFSET)return pas_volt[pas_offset_idx];
    return pas_freq[pas_freq_idx];
}
static void format_champ(char *dst,size_t n,const signal_config_t *s,signal_champ_t champ){
    if(champ==CHAMP_FORME)snprintf(dst,n,"%s",signaux_type_long(s->forme));
    else if(champ==CHAMP_AMPLITUDE)signaux_formater_tension(dst,n,s->amplitude_mvpp,false);
    else if(champ==CHAMP_OFFSET)signaux_formater_tension(dst,n,s->offset_mv,true);
    else signaux_formater_frequence(dst,n,s->frequence_hz);
}
static void afficher_edition(void){
    static const char *titres[]={"MODIFIER TYPE","MODIFIER AMPLITUDE","MODIFIER OFFSET","MODIFIER FREQUENCE"};
    titre(titres[champ_edit]);
    char l[52],val[30],pas[30];
    snprintf(l,sizeof(l),"SIGNAL %02d",c_signal+1);oled_ligne(1,l,false);
    format_champ(val,sizeof(val),actuel(),champ_edit);
    snprintf(l,sizeof(l),"ACTUEL: %s",val);oled_ligne(2,l,false);
    format_champ(val,sizeof(val),&brouillon,champ_edit);
    snprintf(l,sizeof(l),"NOUVEAU: %s",val);oled_ligne(3,l,true);
    if(champ_edit==CHAMP_FORME){
        oled_ligne(4,"SIN / TRI / CAR",false);
    }else if(champ_edit==CHAMP_FREQUENCE){
        signaux_formater_frequence(pas,sizeof(pas),pas_pour(champ_edit));
        snprintf(l,sizeof(l),"PAS: %s",pas);oled_ligne(4,l,false);
    }else{