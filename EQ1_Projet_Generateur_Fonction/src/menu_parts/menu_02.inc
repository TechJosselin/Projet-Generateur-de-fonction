        signaux_formater_tension(pas,sizeof(pas),(int32_t)pas_pour(champ_edit),false);
        snprintf(l,sizeof(l),"PAS: %s",pas);oled_ligne(4,l,false);
    }
    oled_ligne(5,"ROT: CHANGER VALEUR",false);
    oled_ligne(6,"CLIC: VALIDER RAM",false);
    pied("LONG: ANNULER");
}
static int echantillon(signal_forme_t forme,int x){
    static const int sinus[16]={0,38,71,92,100,92,71,38,0,-38,-71,-92,-100,-92,-71,-38};
    static const int triangle[16]={0,25,50,75,100,75,50,25,0,-25,-50,-75,-100,-75,-50,-25};
    int p=((x-2)*32/124)&15;
    if(forme==SIGNAL_SINUS)return sinus[p];
    if(forme==SIGNAL_TRIANGLE)return triangle[p];
    return p<8?100:-100;
}
static void afficher_apercu(void){
    const signal_config_t *s=actuel();if(!s)return;
    char l[48],a[24],f[24],o[24];
    snprintf(l,sizeof(l),"SIG %02d / APERCU",c_signal+1);titre(l);
    signaux_formater_tension(a,sizeof(a),s->amplitude_mvpp,false);
    signaux_formater_tension(o,sizeof(o),s->offset_mv,true);
    signaux_formater_frequence(f,sizeof(f),s->frequence_hz);
    snprintf(l,sizeof(l),"%s %spp",signaux_type_court(s->forme),a);oled_ligne(1,l,false);
    snprintf(l,sizeof(l),"OFFSET: %s",o);oled_ligne(2,l,false);
    snprintf(l,sizeof(l),"FREQ: %s",f);oled_ligne(3,l,false);
    for(int x=2;x<126;x+=4)oled_pixel(x,45);
    int y=45-(echantillon(s->forme,2)*9)/100;
    for(int x=3;x<126;x++){
        int yi=45-(echantillon(s->forme,x)*9)/100;
        oled_segment(x-1,y,x,yi);y=yi;
    }
    pied("SIMU: PAS DE VOUT");
}
static void afficher_sortie(void){
    liste_scrollee("SORTIE ANALOGIQUE",c_sortie,NB_SORTIE,ligne_sortie);
    char s[28];int index=integration_signal_selectionne();
    snprintf(s,sizeof(s),"VOUT: OFF   SIG: %s",index<0?"--":"RAM");
    oled_ligne(1,s,false);
}
static void afficher_sortie_info(void){
    titre("ETAT DE LA SORTIE");
    char l[36];int index=integration_signal_selectionne();
    oled_ligne(1,"SORTIE REELLE: OFF",false);
    snprintf(l,sizeof(l),"SIG. CHOISI: %s",index<0?"AUCUN":"OUI");oled_ligne(2,l,false);
    if(index>=0){snprintf(l,sizeof(l),"NUMERO: %02d",index+1);oled_ligne(3,l,false);}
    oled_ligne(4,integration_ad9833_disponible()?"AD9833: PRET":"AD9833: INDISP.",false);
    oled_ligne(5,integration_epot_disponible()?"EPOT: DRIVER PRET":"EPOT: INDISP.",false);
    oled_ligne(6,"VOUT BLOQUEE/CALIB.",false);
    pied("LONG: RETOUR");
}
static void afficher_digitales(void){
    titre("SORTIES DIGITALES");
    oled_ligne(1,"OUT_DIGITAL_1: ATT.",false);
    oled_ligne(2,"OUT_DIGITAL_2: ATT.",false);
    oled_ligne(4,"PREVUES SCHEMA BLOC",false);
    oled_ligne(5,"NON PILOTEES ICI",false);
    oled_ligne(6,"AUCUN GPIO RESERVE",false);
    pied("LONG: RETOUR");
}
static void afficher_materiel(void){
    titre("CHAINAGE MATERIEL");
    oled_ligne(1,integration_ad9833_disponible()?"AD9833 SPI: PRET":"AD9833 SPI: INDISP",false);
    oled_ligne(2,integration_epot_disponible()?"EPOT I2C: PRET":"EPOT I2C: INDISP",false);
    oled_ligne(3,"EEPROM: ATTENTE",false);
    oled_ligne(4,"LT1206: NON TESTE",false);
    oled_ligne(5,"TRACO: NON TESTE",false);
    oled_ligne(6,"VOUT: NON VALIDEE",false);
    pied("LONG: RETOUR");
}
static void afficher_memoire(void){
    liste_scrollee("MEMOIRE 24LC32",c_memoire,NB_MEMOIRE,ligne_memoire);
    char s[28];snprintf(s,sizeof(s),"RAM %u/100 %s",(unsigned)signaux_nombre(),
        signaux_modifications_en_ram()?"MODIF.":"INIT");oled_ligne(1,s,false);
}
static void afficher_ram_info(void){
    titre("ETAT DE LA RAM");
    char l[32];
    snprintf(l,sizeof(l),"SIGNAUX: %u / 100",(unsigned)signaux_nombre());oled_ligne(1,l,false);
    snprintf(l,sizeof(l),"MODIFS CHAMPS: %u",(unsigned)signaux_nombre_modifies());oled_ligne(2,l,false);
    oled_ligne(3,signaux_modifications_en_ram()?"RAM: MODIFIEE":"RAM: INITIALE",false);
    oled_ligne(4,"EEPROM: NON ACTIVE",false);
    oled_ligne(5,"COUPURE = PERTE RAM",false);
    oled_ligne(6,"SAUVEGARDE A FAIRE",false);
    pied("LONG: RETOUR");
}
static void afficher_usb(void){
    liste_scrollee("COMMUNICATION USB",c_usb,NB_USB,ligne_usb);
    oled_ligne(1,"TRAMES: NON RELIEES",false);
}
static void afficher_usb_info(void){
    titre("USB / TRAMES");
    oled_ligne(1,"PORT SERIE: MONITEUR",false);
    oled_ligne(2,"FORMAT: CDC PAGE 5",false);
    oled_ligne(3,"PARSER C: PREPARE",false);
    oled_ligne(4,"TRANSPORT: A FAIRE",false);
    oled_ligne(5,"EEPROM: A RACCORDER",false);
    oled_ligne(6,"MAX 100: A DEFINIR",false);
    pied("LONG: RETOUR");
}
static void afficher_usb_ecriture(void){
