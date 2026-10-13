    titre("USB / ECRITURE");
    oled_ligne(1,"$N:TYPE:AMP:OFF:FREQ#",false);
    oled_ligne(2,"TYPE=SIN,TRI,CAR",false);
    oled_ligne(3,"AMP: mV CRETE-CRETE",false);
    oled_ligne(4,"OFF: mV SIGNES",false);
    oled_ligne(5,"FREQ: Hz",false);
    oled_ligne(6,"TRANSPORT: NON RELIE",false);
    pied("LONG: RETOUR");
}
static void afficher_usb_lecture(void){
    titre("USB / LECTURE");
    oled_ligne(1,"$N?# -> SIGNAL N",false);
    oled_ligne(2,"$?#  -> TOUS SIG.",false);
    oled_ligne(3,"REPONSE: TRAME ECR.",false);
    oled_ligne(4,"N=1..100 (EXTENSION)",false);
    oled_ligne(5,"TRANSPORT: NON RELIE",false);
    oled_ligne(6,"PARSER RAM: PRET",false);
    pied("LONG: RETOUR");
}
static void afficher_diag_i2c(void){
    char l[32];uint8_t adr=0;
    titre("DIAG 1/4 - I2C");
    oled_ligne(1,integration_i2c_disponible()?"BUS I2C: OK":"BUS I2C: ABSENT",false);
    if(oled_est_present())snprintf(l,sizeof(l),"OLED 0x%02X: OK",oled_adresse());else snprintf(l,sizeof(l),"OLED: ABSENT");oled_ligne(2,l,false);
    snprintf(l,sizeof(l),"GAIN 0x3F: %s",integration_epot_gain_present()?"OK":"ABS");oled_ligne(3,l,false);
    snprintf(l,sizeof(l),"OFFS 0x3E: %s",integration_epot_offset_present()?"OK":"ABS");oled_ligne(4,l,false);
    if(integration_eeprom_detectee(&adr))snprintf(l,sizeof(l),"EEPROM 0x%02X: OK",adr);else snprintf(l,sizeof(l),"EEPROM 0x50: ABS");oled_ligne(5,l,false);
    oled_ligne(6,"ACK I2C TEMPS REEL",false);
    pied("ROT:PAGE LONG:RETOUR");
}
static void afficher_diag_dds(void){
    char l[36],f[24];int index=integration_signal_selectionne();
    titre("DIAG 2/4 - DDS/ANA");
    oled_ligne(1,integration_ad9833_disponible()?"AD9833 SPI: INIT":"AD9833 SPI: ABS",false);
    if(index>=0){
        const signal_config_t *s=signaux_lire((size_t)index);
        if(s){snprintf(l,sizeof(l),"SIG %02d: %s",index+1,signaux_type_court(s->forme));oled_ligne(2,l,false);signaux_formater_frequence(f,sizeof(f),s->frequence_hz);snprintf(l,sizeof(l),"FREQ: %s",f);oled_ligne(3,l,false);}
    }else{
        oled_ligne(2,"SIGNAL: AUCUN",false);
        oled_ligne(3,"FREQ: --",false);
    }
    snprintf(l,sizeof(l),"DDS SORTIE: %s",integration_sortie_reelle_active()?"ON":"OFF");oled_ligne(4,l,false);
    snprintf(l,sizeof(l),"EPOT G/O: %s/%s",integration_epot_gain_present()?"OK":"--",integration_epot_offset_present()?"OK":"--");oled_ligne(5,l,false);
    oled_ligne(6,"AMP/OFF: NON APPLIQUES",false);
    pied("ROT:PAGE LONG:RETOUR");
}
static void afficher_diag_encodeur(void){
    char l[32];
    titre("DIAG 3/4 - ENCODEUR");
    snprintf(l,sizeof(l),"A:%d B:%d SW:%d",gpio_get_level(PIN_ENC_A),gpio_get_level(PIN_ENC_B),gpio_get_level(PIN_ENC_SW));oled_ligne(1,l,false);
    snprintf(l,sizeof(l),"BAS:%"PRIu32" HAUT:%"PRIu32,compteur_plus,compteur_moins);oled_ligne(2,l,false);
    snprintf(l,sizeof(l),"CLICS:%"PRIu32,compteur_clic);oled_ligne(3,l,false);
    snprintf(l,sizeof(l),"LONGS:%"PRIu32,compteur_long);oled_ligne(4,l,false);
    oled_ligne(5,"SENS: DROITE -> BAS",false);
    oled_ligne(6,"IRQ A+B / QUADRATURE",false);
    pied("ROT:PAGE LONG:RETOUR");
}
static void afficher_diag_systeme(void){
    char l[32];uint8_t adr=0;
    titre("DIAG 4/4 - SYSTEME");
    snprintf(l,sizeof(l),"RAM: %u/100 %s",(unsigned)signaux_nombre(),signaux_modifications_en_ram()?"MOD":"OK");oled_ligne(1,l,false);
    if(integration_eeprom_detectee(&adr))snprintf(l,sizeof(l),"EEPROM HW:%02X DRV:OFF",adr);else snprintf(l,sizeof(l),"EEPROM HW:ABS DRV:OFF");oled_ligne(2,l,false);
    oled_ligne(3,integration_usb_trames_disponible()?"USB RX/TX: ACTIF":"USB RX/TX: OFF",false);
    oled_ligne(4,"OUT DIG 1/2: OFF",false);
    oled_ligne(5,"LT1206: TEST MANUEL",false);
    oled_ligne(6,"TRACO: TEST MANUEL",false);
    pied("ROT:PAGE LONG:RETOUR");
}
static void afficher_diagnostic(void){
    switch(c_diag){
    case 0:afficher_diag_i2c();break;
    case 1:afficher_diag_dds();break;
    case 2:afficher_diag_encodeur();break;
    default:afficher_diag_systeme();break;
    }
}
static void afficher_reglages(void){
    liste_scrollee("REGLAGES DE L'UI",c_reglages,NB_REGLAGES,ligne_reglages);
    oled_ligne(1,"PAS REGLABLES",false);
}
static void afficher_pas(void){
    static const char *noms[]={"PAS DE FREQUENCE","PAS D'AMPLITUDE","PAS D'OFFSET"};
    titre(noms[champ_pas]);
    char a[28],b[28],l[52];uint32_t valeur;
    if(champ_pas==0){
        signaux_formater_frequence(a,sizeof(a),pas_freq[pas_freq_idx]);
        valeur=pas_freq[pas_en_cours];
        signaux_formater_frequence(b,sizeof(b),valeur);
    }else{
        int idx=champ_pas==1?pas_ampl_idx:pas_offset_idx;
        signaux_formater_tension(a,sizeof(a),(int32_t)pas_volt[idx],false);
        valeur=pas_volt[pas_en_cours];
        signaux_formater_tension(b,sizeof(b),(int32_t)valeur,false);
    }
    snprintf(l,sizeof(l),"ACTUEL: %s",a);oled_ligne(1,l,false);
    snprintf(l,sizeof(l),"NOUVEAU: %s",b);oled_ligne(2,l,true);
    oled_ligne(4,"ROT: CHANGER PAS",false);
    oled_ligne(5,"CLIC: CONFIRMER",false);
    oled_ligne(6,"LONG: ANNULER",false);
    pied("EDITION RAM");
}
static void afficher_infos(void){
    titre("INFOS / PROJET");
    oled_ligne(1,"CFPT LEADERSHIP",false);
    oled_ligne(2,"XIAO ESP32C6",false);
    oled_ligne(3,"OLED SH1106 128X64",false);
    oled_ligne(4,"ENCODEUR PEC12R IRQ",false);
    oled_ligne(5,"INTEGRATION UI V4",false);
    oled_ligne(6,"DDS TEST / ANA CALIB",false);
    pied("LONG: RETOUR");
}
static void afficher_confirm(void){
    titre("CONFIRMATION");
    const char *libelle="VALIDER ACTION ?";
    const char *precision="CONFIRMATION REQUISE";
    switch(confirmation){
    case CONFIRMER_SUPPRESSION:libelle="SUPPRIMER SIGNAL ?";precision="SUPPRESSION EN RAM";break;
    case CONFIRMER_USINE:libelle="RESTAURER USINE ?";precision="10 SIGNAUX EN RAM";break;
    case CONFIRMER_SAUVER:libelle="SAUVER EEPROM ?";precision="ECRASER ANCIENNES ?";break;
    case CONFIRMER_CHARGER:libelle="CHARGER EEPROM ?";precision="REMPLACER RAM ?";break;
    case CONFIRMER_VOUT_ON:libelle="ACTIVER DDS ?";precision="TEST AU SCOPE / BRUT";break;
    default:break;
    }
    oled_ligne(1,libelle,false);
    oled_ligne(2,precision,false);
    oled_ligne(3,"CHOISIR NON / OUI",false);
    oled_ligne(4,"NON",!choix_oui);
    oled_ligne(5,"OUI",choix_oui);
    pied("ROT: CHOIX CLIC:OK");
}
static void afficher_message(void){
    titre(titre_message);
    oled_ligne(2,ligne_message1,false);
    oled_ligne(3,ligne_message2,false);
    pied("CLIC: CONTINUER");
}
static void afficher(void){
    oled_effacer();
    switch(page){
    case PAGE_ACCUEIL:afficher_accueil();break;
    case PAGE_LISTE:afficher_liste();break;
    case PAGE_DETAIL:afficher_detail();break;
    case PAGE_EDITION:afficher_edition();break;
    case PAGE_APERCU:afficher_apercu();break;
    case PAGE_SORTIE:afficher_sortie();break;
