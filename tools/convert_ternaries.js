/* Transformations explicites des expressions ternaires du firmware existant.
 * Destinees a une application unique sur l'etat precedent au refactoring.
 */
const removeTernaries = (function () {
    function rewrite(s, n) {
        const replace = (pattern, value) => { s = s.replace(pattern, value); };
        switch (n) {
            case "ad9833":
                replace(/return t\?t:1;/, "if (t) return t;\nreturn 1;");
                replace(/return meme && configure\?ESP_OK:ESP_ERR_INVALID_STATE;/, "if (meme && configure) return ESP_OK;\nreturn ESP_ERR_INVALID_STATE;");
                replace(/uint16_t bits=f==AD9833_WAVE_TRIANGLE\?0x0002U:\(f==AD9833_WAVE_SQUARE\?0x0028U:0U\);/, "uint16_t bits = 0;\nif (f == AD9833_WAVE_TRIANGLE) bits = 0x0002U;\nelse if (f == AD9833_WAVE_SQUARE) bits = 0x0028U;");
                replace(/uint16_t c=enable\?\(uint16_t\)\(controle&~RESET\):\(uint16_t\)\(controle\|RESET\);/, "uint16_t c;\nif (enable) c = (uint16_t)(controle & ~RESET);\nelse c = (uint16_t)(controle | RESET);");
                break;
            case "analogique":
                replace(/return a>b\?a-b:b-a;/, "if (a > b) return a - b;\nreturn b - a;");
                replace(/return a<b\?\(x>=a && x<=b\):\(x>=b && x<=a\);/, "if (a < b) return x >= a && x <= b;\nreturn x >= b && x <= a;");
                replace(/\(croissant\?g->amplitude_mvpp<=v->gain\[i-1\]\.amplitude_mvpp:\s*g->amplitude_mvpp>=v->gain\[i-1\]\.amplitude_mvpp\)/, "((croissant && g->amplitude_mvpp <= v->gain[i-1].amplitude_mvpp) || (!croissant && g->amplitude_mvpp >= v->gain[i-1].amplitude_mvpp))");
                replace(/\(croissant\?o->delta_mv<=v->offset\[i-1\]\.delta_mv:\s*o->delta_mv>=v->offset\[i-1\]\.delta_mv\)/, "((croissant && o->delta_mv <= v->offset[i-1].delta_mv) || (!croissant && o->delta_mv >= v->offset[i-1].delta_mv))");
                replace(/return disponibles\?GF_OK:GF_ARGUMENT;/, "if (disponibles) return GF_OK;\nreturn GF_ARGUMENT;");
                replace(/b\[7\]=p->gain_avant_offset\?1:0;/, "b[7] = 0;\nif (p->gain_avant_offset) b[7] = 1;");
                break;
            case "eeprom":
                replace(/return e==ESP_OK\?GF_OK:\(e==ESP_ERR_TIMEOUT\?GF_TIMEOUT:GF_IO\);/, "if (e == ESP_OK) return GF_OK;\nif (e == ESP_ERR_TIMEOUT) return GF_TIMEOUT;\nreturn GF_IO;");
                replace(/if\(device\)return b==bus && a==adresse\?ESP_OK:ESP_ERR_INVALID_STATE;/, "if (device) {\nif (b == bus && a == adresse) return ESP_OK;\nreturn ESP_ERR_INVALID_STATE;\n}");
                replace(/size_t taille=n>128\?128:n;/, "size_t taille = n;\nif (n > 128) taille = 128;");
                break;
            case "entree":
                replace(/q->somme=p\?q->somme\+p:0;/, "if (p) q->somme += p;\nelse q->somme = 0;");
                replace(/int r=q->somme>=4\?1:\(q->somme<=-4\?-1:0\);/, "int r = 0;\nif (q->somme >= 4) r = 1;\nelse if (q->somme <= -4) r = -1;");
                break;
            case "epot":
                replace(/return e!=ESP_OK\?e:\(lu==v\?ESP_OK:ESP_ERR_INVALID_RESPONSE\);/, "if (e != ESP_OK) return e;\nif (lu == v) return ESP_OK;\nreturn ESP_ERR_INVALID_RESPONSE;");
                replace(/if\(initialise\)return bus==bus_initial && ga==adresses\[0\] && oa==adresses\[1\]\?ESP_OK:ESP_ERR_INVALID_STATE;/, "if (initialise) {\nif (bus == bus_initial && ga == adresses[0] && oa == adresses[1]) return ESP_OK;\nreturn ESP_ERR_INVALID_STATE;\n}");
                break;
            case "gf":
                replace(/return \(unsigned\)r<sizeof\(names\)\/sizeof\(names\[0\]\)\?names\[r\]:"INCONNU";/, "if ((unsigned)r < sizeof(names) / sizeof(names[0])) return names[r];\nreturn \"INCONNU\";");
                replace(/for\(unsigned b=0;b<8;b\+\+\)c=\(c>>1\)\^\(\(c&1U\)\?UINT32_C\(0xEDB88320\):0\);/, "for (unsigned b = 0; b < 8; b++) {\nuint32_t poly = 0;\nif (c & 1U) poly = UINT32_C(0xEDB88320);\nc = (c >> 1) ^ poly;\n}");
                break;
            case "integration":
                replace(/etat\.etat=!hw\.dds\?SYS_NON_INITIALISE:\s*\(calibration_valider\(&profil\)!=GF_OK\?SYS_NON_CALIBRE:\s*\(etat\.selection>=0\?SYS_PRET:SYS_ARRET_COMMANDE\)\);/, "if (!hw.dds) etat.etat = SYS_NON_INITIALISE;\nelse if (calibration_valider(&profil) != GF_OK) etat.etat = SYS_NON_CALIBRE;\nelse if (etat.selection >= 0) etat.etat = SYS_PRET;\nelse etat.etat = SYS_ARRET_COMMANDE;");
                replace(/gf_result_t r=hw\.dds && hw\.arreter\?hw\.arreter\(hw\.ctx\):GF_ABSENT;/, "gf_result_t r = GF_ABSENT;\nif (hw.dds && hw.arreter) r = hw.arreter(hw.ctx);");
                replace(/etat\.chargement=hw\.eeprom\?stockage_charger\(&memoire,&instantane\):GF_ABSENT;/, "etat.chargement = GF_ABSENT;\nif (hw.eeprom) etat.chargement = stockage_charger(&memoire, &instantane);");
                replace(/etat\.sauvegarde=hw\.eeprom\?GF_OK:GF_ABSENT;/, "etat.sauvegarde = GF_ABSENT;\nif (hw.eeprom) etat.sauvegarde = GF_OK;");
                replace(/etat\.sauvegarde=hw\.eeprom\?stockage_commencer\(&memoire,&instantane\):GF_ABSENT;/, "etat.sauvegarde = GF_ABSENT;\nif (hw.eeprom) etat.sauvegarde = stockage_commencer(&memoire, &instantane);");
                replace(/etat\.erreur_arret=hw\.arreter\?hw\.arreter\(hw\.ctx\):GF_ABSENT;/, "etat.erreur_arret = GF_ABSENT;\nif (hw.arreter) etat.erreur_arret = hw.arreter(hw.ctx);");
                replace(/const signal_config_t \*s=etat\.selection>=0\?signaux_lire\(\(size_t\)etat\.selection\):NULL;/, "const signal_config_t *s = NULL;\nif (etat.selection >= 0) s = signaux_lire((size_t)etat.selection);");
                replace(/bool ok=v\?signaux_modifier\(i,s\):signaux_ajouter\(s\);/, "bool ok;\nif (v) ok = signaux_modifier(i, s);\nelse ok = signaux_ajouter(s);");
                break;
            case "menu":
                replace(/index_signal=n\?borne\(index_signal,0,\(int\)n-1\):0;/, "if (n) index_signal = borne(index_signal, 0, (int)n - 1);\nelse index_signal = 0;");
                replace(/int max_premiere=total>3\?total-3:0;/, "int max_premiere = 0;\nif (total > 3) max_premiere = total - 3;");
                replace(/decalage=phase>max_decalage\?max_decalage:phase;/, "if (phase > max_decalage) decalage = max_decalage;\nelse decalage = phase;");
                replace(/dds_seul\?"DDS":nominal\?"NOM":"ON"/, "label_liste");
                replace(/st->etat==SYS_DEFAUT\?"DEFAUT":\(actif && st->selection==index_signal\?\(dds_seul\?"DDS SEUL":nominal\?"NOM":"ON"\):"OFF"\)/, "label_detail");
                replace(/erreur_arret\?"ARRET NON CONFIRME":"SORTIE NON ACTIVEE"/, "label_erreur");
                replace(/switch\(page\) \{/, "const char *label_liste = \"ON\";\nif (dds_seul) label_liste = \"DDS\";\nelse if (nominal) label_liste = \"NOM\";\nconst char *label_detail = \"OFF\";\nif (st->etat == SYS_DEFAUT) label_detail = \"DEFAUT\";\nelse if (actif && st->selection == index_signal) {\nif (dds_seul) label_detail = \"DDS SEUL\";\nelse if (nominal) label_detail = \"NOM\";\nelse label_detail = \"ON\";\n}\nconst char *label_erreur = \"SORTIE NON ACTIVEE\";\nif (erreur_arret) label_erreur = \"ARRET NON CONFIRME\";\nswitch(page) {");
                break;
            case "oled":
                replace(/int x0=x<0\?0:x,y0=y<0\?0:y;/, "int x0 = x, y0 = y;\nif (x < 0) x0 = 0;\nif (y < 0) y0 = 0;");
                replace(/int x1=droite>128\?128:\(int\)droite,y1=bas>64\?64:\(int\)bas;/, "int x1 = (int)droite, y1 = (int)bas;\nif (droite > 128) x1 = 128;\nif (bas > 64) y1 = 64;");
                replace(/int dx=x1>x\?x1-x:x-x1,dy=y1>y\?y1-y:y-y1,sx=x<x1\?1:-1,sy=y<y1\?1:-1,e=dx-dy;/, "int dx = x1 - x;\nif (x1 <= x) dx = x - x1;\nint dy = y1 - y;\nif (y1 <= y) dy = y - y1;\nint sx = -1;\nif (x < x1) sx = 1;\nint sy = -1;\nif (y < y1) sy = 1;\nint e = dx - dy;");
                break;
            case "signaux":
                replace(/s->forme=\(signal_forme_t\)\(f<0\?2:\(f>2\?0:f\)\);/, "int forme = f;\nif (f < 0) forme = 2;\nelse if (f > 2) forme = 0;\ns->forme = (signal_forme_t)forme;");
                replace(/int64_t v=champ==CHAMP_AMPLITUDE\?s->amplitude_mvpp:s->offset_mv;/, "int64_t v;\nif (champ == CHAMP_AMPLITUDE) v = s->amplitude_mvpp;\nelse v = s->offset_mv;");
                replace(/int64_t mini=champ==CHAMP_AMPLITUDE\?SIGNAL_AMPL_MIN_MVPP:SIGNAL_OFFSET_MIN_MV;/, "int64_t mini;\nif (champ == CHAMP_AMPLITUDE) mini = SIGNAL_AMPL_MIN_MVPP;\nelse mini = SIGNAL_OFFSET_MIN_MV;");
                replace(/int64_t maxi=champ==CHAMP_AMPLITUDE\?SIGNAL_AMPL_MAX_MVPP:SIGNAL_OFFSET_MAX_MV;/, "int64_t maxi;\nif (champ == CHAMP_AMPLITUDE) maxi = SIGNAL_AMPL_MAX_MVPP;\nelse maxi = SIGNAL_OFFSET_MAX_MV;");
                replace(/v\+=\(int64_t\)sens\*\(int64_t\)\(pas\?pas:100\);/, "uint32_t pas_effectif = 100;\nif (pas) pas_effectif = pas;\nv += (int64_t)sens * (int64_t)pas_effectif;");
                replace(/int64_t v=\(int64_t\)s->frequence_hz\+\(int64_t\)sens\*\(int64_t\)\(pas\?pas:1\);/, "uint32_t pas_effectif = 1;\nif (pas) pas_effectif = pas;\nint64_t v = (int64_t)s->frequence_hz + (int64_t)sens * (int64_t)pas_effectif;");
                replace(/char sign=v<0\?'-':'\+';/, "char sign = '+';\nif (v < 0) sign = '-';");
                break;
            case "sorties_digitales":
                replace(/esp_err_t e=configures\[c\]\?ledc_stop\(LEDC_LOW_SPEED_MODE,\(ledc_channel_t\)c,0\):gpio_set_level\(pins\[c\],0\);/, "esp_err_t e;\nif (configures[c]) e = ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)c, 0);\nelse e = gpio_set_level(pins[c], 0);");
                replace(/return e==ESP_OK\?GF_OK:GF_IO;/, "if (e == ESP_OK) return GF_OK;\nreturn GF_IO;");
                replace(/uint32_t diff=real>hz\?real-hz:hz-real;/, "uint32_t diff = hz - real;\nif (real > hz) diff = real - hz;");
                replace(/\.duty=\(duty==1000\)\?0:\(uint32_t\)\(\(\(\(uint64_t\)1<<bits\)\*duty\+500\)\/1000\)/, ".duty = 0");
                replace(/\/\* Arrondir une consigne presque pleine sans depasser le compteur\. \*\//, "/* Calcule le rapport cyclique sans modifier le traitement LEDC de 100 %. */\nif (duty != 1000) ch.duty = (uint32_t)((((uint64_t)1 << bits) * duty + 500) / 1000);\n/* Arrondir une consigne presque pleine sans depasser le compteur. */");
                replace(/return e==ESP_ERR_INVALID_ARG\?GF_PLAGE:GF_IO;/, "if (e == ESP_ERR_INVALID_ARG) return GF_PLAGE;\nreturn GF_IO;");
                break;
            case "stockage":
                replace(/return tout_vierge\?GF_VIDE:erreur;/, "if (tout_vierge) return GF_VIDE;\nreturn erreur;");
                replace(/s->cible=s->actif==0\?1:0;/, "s->cible = 0;\nif (s->actif == 0) s->cible = 1;");
                replace(/terminer\(s,r!=GF_OK\?r:GF_VERIFICATION\);/g, "gf_result_t cause = GF_VERIFICATION;\nif (r != GF_OK) cause = r;\nterminer(s, cause);");
                break;
            case "usb_protocol":
                replace(/int64_t x=neg\?-\(int64_t\)v:\(int64_t\)v;/, "int64_t x = (int64_t)v;\nif (neg) x = -(int64_t)v;");
                replace(/c\.action=!strcmp\(b,"SELECT"\)\?USB_CHOISIR:\(!strcmp\(b,"DELETE"\)\?USB_SUPPRIMER:USB_DUPLIQUER\);/, "if (!strcmp(b, \"SELECT\")) c.action = USB_CHOISIR;\nelse if (!strcmp(b, \"DELETE\")) c.action = USB_SUPPRIMER;\nelse c.action = USB_DUPLIQUER;");
                break;
            case "usb_service":
                replace(/s->modifie\?1U:0U,s->sauvegarde_en_cours\?1U:0U/, "a_modifie, en_sauvegarde");
                replace(/integration_sortie_digitale_disponible\(canal\)\?1U:0U,d->connu\?1U:0U,d->actif\?1U:0U/, "disponible, connu, actif");
                replace(/return \*n\?tx\+debut:NULL;/, "if (*n) return tx + debut;\nreturn NULL;");
                replace(/const integration_statut_t \*s=integration_statut\(\);/, "const integration_statut_t *s = integration_statut();\nunsigned a_modifie = 0, en_sauvegarde = 0;\nif (s->modifie) a_modifie = 1U;\nif (s->sauvegarde_en_cours) en_sauvegarde = 1U;");
                replace(/const digital_statut_t \*d=integration_digital_statut\(canal\);/, "const digital_statut_t *d = integration_digital_statut(canal);\nunsigned disponible = 0, connu = 0, actif = 0;\nif (integration_sortie_digitale_disponible(canal)) disponible = 1U;\nif (d->connu) connu = 1U;\nif (d->actif) actif = 1U;");
                break;
        }
        return s;
    }
    return rewrite;
})();
removeTernaries;
