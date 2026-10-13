    case PAGE_DETAIL:selection_detail();break;
    case PAGE_EDITION:
        if(signaux_modifier((size_t)c_signal,brouillon))
            message("VALEUR VALIDEE","MODIFIE EN RAM","EEPROM NON SAUVEE",PAGE_DETAIL);
        else message("VALEUR REFUSEE","HORS LIMITES UI","",PAGE_DETAIL);
        break;
    case PAGE_APERCU:retour();break;
    case PAGE_SORTIE:
        switch(c_sortie){
        case 0:aller(PAGE_SORTIE_INFO);break;
        case 1:
            confirmation=CONFIRMER_VOUT_ON;choix_oui=false;aller(PAGE_CONFIRM);
            break;
        case 2:
            if(integration_couper_sortie())message("VOUT DESACTIVEE","SORTIE OFF","",PAGE_SORTIE);
            else message("ERREUR VOUT","COUPURE NON CONF.","",PAGE_SORTIE);
            break;
        case 3:aller(PAGE_DIGITALES);break;
        case 4:aller(PAGE_MATERIEL);break;
        default:retour();break;
        }break;
    case PAGE_SORTIE_INFO:case PAGE_DIGITALES:case PAGE_MATERIEL:retour();break;
    case PAGE_MEMOIRE:
        switch(c_memoire){
        case 0:aller(PAGE_RAM_INFO);break;
        case 1:confirmation=CONFIRMER_SAUVER;choix_oui=false;aller(PAGE_CONFIRM);break;
        case 2:confirmation=CONFIRMER_CHARGER;choix_oui=false;aller(PAGE_CONFIRM);break;
        case 3:confirmation=CONFIRMER_USINE;choix_oui=false;aller(PAGE_CONFIRM);break;
        default:retour();break;
        }break;
    case PAGE_RAM_INFO:retour();break;
    case PAGE_USB:
        switch(c_usb){
        case 0:aller(PAGE_USB_INFO);break;
        case 1:aller(PAGE_USB_ECRITURE);break;
        case 2:aller(PAGE_USB_LECTURE);break;
        default:retour();break;
        }break;
    case PAGE_USB_INFO:case PAGE_USB_ECRITURE:case PAGE_USB_LECTURE:retour();break;
    case PAGE_DIAGNOSTIC:retour();break;
    case PAGE_REGLAGES:
        if(c_reglages==3)retour();
        else{
            champ_pas=c_reglages;
            pas_en_cours=champ_pas==0?pas_freq_idx:
                         (champ_pas==1?pas_ampl_idx:pas_offset_idx);
            apres_pas=PAGE_REGLAGES;
            aller(PAGE_EDIT_PAS);
        }break;
    case PAGE_EDIT_PAS:
        if(champ_pas==0)pas_freq_idx=pas_en_cours;
        else if(champ_pas==1)pas_ampl_idx=pas_en_cours;
        else pas_offset_idx=pas_en_cours;
        aller(apres_pas);break;
    case PAGE_INFOS:retour();break;
    case PAGE_CONFIRM:valider_confirmation();break;
    case PAGE_MESSAGE:aller(apres_message);break;
    }
}
void menu_traiter(evenement_ui_t e){
    if(e==UI_AUCUN)return;
    if(e==UI_ROTATION_PLUS)compteur_plus++;
    if(e==UI_ROTATION_MOINS)compteur_moins++;
    if(e==UI_CLIC)compteur_clic++;
    if(e==UI_APPUI_LONG){retour();return;}
    if(e==UI_CLIC){valider();redessiner=true;return;}
    if(e!=UI_ROTATION_PLUS && e!=UI_ROTATION_MOINS)return;
    int d=e==UI_ROTATION_PLUS?1:-1;
    switch(page){
    case PAGE_ACCUEIL:c_accueil=borne(c_accueil+d,0,NB_ACCUEIL-1);break;
    case PAGE_LISTE:c_signal=borne(c_signal+d,0,(int)signaux_nombre()-1);break;
    case PAGE_DETAIL:c_detail=borne(c_detail+d,0,NB_DETAIL-1);break;
    case PAGE_EDITION:signaux_variation(&brouillon,champ_edit,d,pas_pour(champ_edit));break;
    case PAGE_SORTIE:c_sortie=borne(c_sortie+d,0,NB_SORTIE-1);break;
    case PAGE_MEMOIRE:c_memoire=borne(c_memoire+d,0,NB_MEMOIRE-1);break;
    case PAGE_USB:c_usb=borne(c_usb+d,0,NB_USB-1);break;
    case PAGE_REGLAGES:c_reglages=borne(c_reglages+d,0,NB_REGLAGES-1);break;
    case PAGE_EDIT_PAS:
        pas_en_cours=borne(pas_en_cours+d,0,champ_pas==0?PAS_FREQ_N-1:PAS_VOLT_N-1);break;
    case PAGE_CONFIRM:choix_oui=d>0;break;
    default:return;
    }
    redessiner=true;
}
void menu_periodique(void){
    int64_t maintenant=esp_timer_get_time();
    if(page==PAGE_MESSAGE && maintenant>=message_jusque)aller(apres_message);
    if(page==PAGE_DIAGNOSTIC && maintenant-dernier_diag>=250000LL){
        dernier_diag=maintenant;redessiner=true;
    }
    if(redessiner)afficher();
}
