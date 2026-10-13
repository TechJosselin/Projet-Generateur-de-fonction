    case PAGE_SORTIE_INFO:afficher_sortie_info();break;
    case PAGE_DIGITALES:afficher_digitales();break;
    case PAGE_MATERIEL:afficher_materiel();break;
    case PAGE_MEMOIRE:afficher_memoire();break;
    case PAGE_RAM_INFO:afficher_ram_info();break;
    case PAGE_USB:afficher_usb();break;
    case PAGE_USB_INFO:afficher_usb_info();break;
    case PAGE_USB_ECRITURE:afficher_usb_ecriture();break;
    case PAGE_USB_LECTURE:afficher_usb_lecture();break;
    case PAGE_DIAGNOSTIC:afficher_diagnostic();break;
    case PAGE_REGLAGES:afficher_reglages();break;
    case PAGE_EDIT_PAS:afficher_pas();break;
    case PAGE_INFOS:afficher_infos();break;
    case PAGE_CONFIRM:afficher_confirm();break;
    case PAGE_MESSAGE:afficher_message();break;
    }
    oled_actualiser();
    redessiner=false;
}
void menu_initialiser(void){
    page=PAGE_ACCUEIL;
    c_accueil=c_signal=c_detail=c_sortie=c_memoire=c_usb=c_reglages=0;
    pas_freq_idx=3;pas_ampl_idx=2;pas_offset_idx=2;
    compteur_plus=compteur_moins=compteur_clic=0;
    redessiner=true;
}
static void valider_confirmation(void){
    confirmer_t action=confirmation;
    if(!choix_oui){retour();return;}
    if(action==CONFIRMER_SUPPRESSION){
        if(signaux_supprimer((size_t)c_signal)){
            integration_annuler_selection(); /* index invalide apres decalage */
            c_signal=borne(c_signal,0,(int)signaux_nombre()-1);
            c_detail=0;
            message("SIGNAL SUPPRIME","SUPPRIME EN RAM","EEPROM INCHANGEE",PAGE_LISTE);
        }else message("SUPPRESSION REFUSEE","GARDER 1 SIGNAL MIN.","",PAGE_DETAIL);
    }else if(action==CONFIRMER_USINE){
        signaux_retablir_usine();
        integration_annuler_selection();
        c_signal=c_detail=0;
        message("PRESETS RESTAURES","10 SIG. D'USINE RAM","EEPROM INCHANGEE",PAGE_MEMOIRE);
    }else if(action==CONFIRMER_SAUVER){
        if(integration_eeprom_sauvegarder())
            message("EEPROM OK","PARAMETRES SAUVES","",PAGE_MEMOIRE);
        else message("EEPROM INDISP.","AUCUNE ECRITURE","RAM CONSERVEE",PAGE_MEMOIRE);
    }else if(action==CONFIRMER_CHARGER){
        if(integration_eeprom_charger()){
            integration_annuler_selection();
            c_signal=c_detail=0;
            message("EEPROM CHARGEE","VERIFIER PRESETS","",PAGE_MEMOIRE);
        }else message("EEPROM INDISP.","AUCUNE LECTURE","RAM CONSERVEE",PAGE_MEMOIRE);
    }else if(action==CONFIRMER_VOUT_ON){
        if(integration_activer_sortie())
            message("VOUT ACTIVE","VERIFIER OSCILLO","",PAGE_SORTIE);
        else message("VOUT NON DISPONIBLE","PILOTES A INTEGRER","SORTIE OFF",PAGE_SORTIE);
    }
}
static void selection_detail(void){
    switch(c_detail){
    case 0:{
        integration_selection_t r=integration_selectionner((size_t)c_signal,actuel());
        if(r==INTEGRATION_SELECTION_DDS_PREPAREE)
            message("SIGNAL PREPARE","AD9833 CONFIGURE","VOUT RESTE OFF",PAGE_DETAIL);
        else if(r==INTEGRATION_SELECTION_RAM)
            message("SIGNAL CHOISI","SELECTION EN RAM","AD9833 INDISP.",PAGE_DETAIL);
        else message("ERREUR SIGNAL","PARAMETRE INVALIDE","",PAGE_DETAIL);
        break;
    }
    case 1:case 2:case 3:case 4:
        champ_edit=(signal_champ_t)(c_detail-1);
        brouillon=*actuel();
        aller(PAGE_EDITION);break;
    case 5:aller(PAGE_APERCU);break;
    case 6:{
        int idx=signaux_dupliquer((size_t)c_signal);
        if(idx<0)message("IMPOSSIBLE","LIMITE 100 SIGNAUX","",PAGE_DETAIL);
        else{
            c_signal=idx;c_detail=0;
            message("SIGNAL DUPLIQUE","AJOUTE EN FIN DE LISTE","RAM UNIQUEMENT",PAGE_DETAIL);
        }
        break;
    }
    case 7:
        confirmation=CONFIRMER_SUPPRESSION;choix_oui=false;aller(PAGE_CONFIRM);break;
    default:retour();break;
    }
}
static void valider(void){
    switch(page){
    case PAGE_ACCUEIL:
        switch(c_accueil){
        case 0:aller(PAGE_LISTE);break;
        case 1:aller(PAGE_SORTIE);break;
        case 2:aller(PAGE_MEMOIRE);break;
        case 3:aller(PAGE_USB);break;
        case 4:aller(PAGE_DIAGNOSTIC);break;
        case 5:aller(PAGE_REGLAGES);break;
        default:aller(PAGE_INFOS);break;
        }break;
    case PAGE_LISTE:c_detail=0;aller(PAGE_DETAIL);break;
