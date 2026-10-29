        if(confirmation==CONFIRMER_SUPPRESSION)aller(PAGE_DETAIL);
        else if(confirmation==CONFIRMER_VOUT_ON)aller(PAGE_SORTIE);
        else aller(PAGE_MEMOIRE);
        break;
    case PAGE_MESSAGE:aller(apres_message);break;
    default:break;
    }
}
/* Bandeau fixe + exactement 3 titres agrandis (128x64 px).
 * Navigation continue avec l'encodeur, la fenetre avance seulement au besoin.
 * Une etiquette trop longue defile horizontalement UNIQUEMENT si selectionnee.
 */
static void liste_scrollee(const char *titre_s,int curseur,int total,
                           void (*fabriquer)(int,char *,size_t)){
    titre(titre_s); /* Toujours a y=0, ne bouge jamais lors des rotations. */
    oled_segment(0,10,127,10);
    if(total<=0){defilement_necessaire=false;return;}
    const int maxi_premiere=total>3?total-3:0;
    if(page_fenetre!=page){
        page_fenetre=page;
        premiere_ligne_visible=borne(curseur-1,0,maxi_premiere);
    }
    if(curseur<premiere_ligne_visible) premiere_ligne_visible=curseur;
    if(curseur>=premiere_ligne_visible+3) premiere_ligne_visible=curseur-2;
    premiere_ligne_visible=borne(premiere_ligne_visible,0,maxi_premiere);

    const int64_t maintenant=esp_timer_get_time();
    defilement_necessaire=false;
    for(int i=0;i<3;i++){
        int index=premiere_ligne_visible+i;
        if(index>=total)break;
        char ligne[64];
        fabriquer(index,ligne,sizeof(ligne));
        const bool choisi=index==curseur;
        size_t longueur=strlen(ligne);
        const char *visible=ligne;
        char tampon[OLED_GRAND_NB_CARACTERES+1];
        int decalage=0;
        if(longueur>OLED_GRAND_NB_CARACTERES){
            if(choisi){
                defilement_necessaire=true;
                int64_t ecoule=maintenant-dernier_evenement_us;
                if(ecoule>900000LL){
                    /* Deux pauses a la fin pour laisser lire l'etiquette. */
                    const int avance=(int)((ecoule-900000LL)/350000LL);
                    const int max_decalage=(int)longueur-OLED_GRAND_NB_CARACTERES;
                    int phase=avance%(max_decalage+4);
                    decalage=phase>max_decalage?max_decalage:phase;
                }
                visible=ligne+decalage;
            }else{
                /* Un > signale qu'il reste du texte hors de l'ecran. */
                memcpy(tampon,ligne,OLED_GRAND_NB_CARACTERES-1);
                tampon[OLED_GRAND_NB_CARACTERES-1]='>';
                tampon[OLED_GRAND_NB_CARACTERES]=0;
                visible=tampon;
            }
        }
        /* 14 px pour les caracteres, 3 px entre les lignes. */
        const int y=14+i*17;
        oled_texte_grand(4,y,visible);
        if(choisi)oled_inverser_zone(0,y-1,128,16);
    }
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
    liste_scrollee("GENERATEUR SIGNAL",c_accueil,NB_ACCUEIL,ligne_accueil);
}
static void afficher_liste(void){
    liste_scrollee("SIGNAUX / LISTE",c_signal,(int)signaux_nombre(),ligne_signal);
}
static void afficher_detail(void){
    liste_scrollee("SIGNAL / OPTIONS",c_detail,NB_DETAIL,ligne_detail);
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