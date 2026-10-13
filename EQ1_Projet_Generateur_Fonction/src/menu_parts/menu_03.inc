    titre("USB / ECRITURE");
    oled_ligne(1,"$N:TYPE:AMP:OFF:FREQ#",false);
    oled_ligne(2,"TYPE=SIN,TRI,CAR",false);
    oled_ligne(3,"AMP: mV CRETE-CRETE",false);
    oled_ligne(4,"OFF: mV SIGNES",false);
    oled_ligne(5,"FREQ: Hz",false);
    oled_ligne(6,"TEST PAR PC / RAM",false);
    pied("LONG: RETOUR");
}
static void afficher_usb_lecture(void){
    titre("USB / LECTURE");
    oled_ligne(1,"$N?# -> SIGNAL N",false);
    oled_ligne(2,"$?#  -> TOUS SIG.",false);
    oled_ligne(3,"REPONSE: TRAME ECR.",false);
    oled_ligne(4,"N=1..100: EXTENSION",false);
    oled_ligne(5,"CDC: 1 CARACTERES",false);
    oled_ligne(6,"PROTO A VALIDER",false);
    pied("LONG: RETOUR");
}
static void afficher_diagnostic(void){
    titre("DIAGNOSTIC GPIO");
    char l[32];
    snprintf(l,sizeof(l),"OLED %s 0x%02X",oled_est_present()?"OK":"ABS",oled_adresse());oled_ligne(1,l,false);
    snprintf(l,sizeof(l),"A:%d B:%d SW:%d",gpio_get_level(PIN_ENC_A),
             gpio_get_level(PIN_ENC_B),gpio_get_level(PIN_ENC_SW));oled_ligne(2,l,false);
    snprintf(l,sizeof(l),"ROT+:%"PRIu32"  -:%"PRIu32,compteur_plus,compteur_moins);oled_ligne(3,l,false);
    snprintf(l,sizeof(l),"CLICS:%"PRIu32,compteur_clic);oled_ligne(4,l,false);
    oled_ligne(5,"I2C: OLED CONNECTE",false);
    oled_ligne(6,"AUTRES CI: NON TEST",false);
    pied("LONG: RETOUR");
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
    oled_ligne(4,"ENCODEUR PEC12R",false);
    oled_ligne(5,"INTERFACE C V3",false);
    oled_ligne(6,"VOUT DESACTIVEE",false);
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
    case CONFIRMER_VOUT_ON:libelle="ACTIVER VOUT ?";precision="VERIFIER CONNEXIONS";break;
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
