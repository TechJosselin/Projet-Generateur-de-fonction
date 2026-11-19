/* Outil ponctuel de mise en forme du firmware du projet Equipe 1.
 * Les transformations sur les expressions ternaires sont explicites et localisees.
 * Les autres changements concernent la presentation et les accolades.
 * Ne pas appliquer a des fichiers sans revue / compilation.
 */
const readability = (function () {
    function lex(s) {
        const t = [];
        let i = 0, beginning = true;
        while (i < s.length) {
            const c = s[i], start = i;
            if (c === "\n") { beginning = true; i++; continue; }
            if (c === " " || c === "\t" || c === "\r") { i++; continue; }
            if (beginning && c === "#") {
                while (i < s.length) {
                    if (s[i] === "\n") {
                        if (s[i - 1] === "\\") { i++; continue; }
                        break;
                    }
                    i++;
                }
                t.push({v: s.slice(start, i), k: "pp"});
                beginning = false;
                continue;
            }
            beginning = false;
            if (c === "/" && s[i + 1] === "/") {
                i += 2;
                while (i < s.length && s[i] !== "\n") i++;
                t.push({v: s.slice(start, i), k: "comment"});
                continue;
            }
            if (c === "/" && s[i + 1] === "*") {
                i += 2;
                while (i < s.length && !(s[i] === "*" && s[i + 1] === "/")) i++;
                i = Math.min(s.length, i + 2);
                t.push({v: s.slice(start, i), k: "comment"});
                continue;
            }
            if (c === "'" || c === '"') {
                const q = c;
                i++;
                while (i < s.length) {
                    if (s[i] === "\\") { i += 2; continue; }
                    if (s[i] === q) { i++; break; }
                    i++;
                }
                t.push({v: s.slice(start, i), k: "lit"});
                continue;
            }
            if (/[A-Za-z_0-9]/.test(c)) {
                i++;
                while (i < s.length && /[A-Za-z_0-9]/.test(s[i])) i++;
                t.push({v: s.slice(start, i), k: "id"});
                continue;
            }
            const ops = ["<<=", ">>=", "...", "->", "==", "!=", "<=", ">=", "&&", "||", "++", "--", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<", ">>", "##"];
            const op = ops.find(x => s.startsWith(x, i));
            if (op) {
                i += op.length;
                t.push({v: op, k: "sym"});
            } else {
                t.push({v: c, k: "sym"});
                i++;
            }
        }
        return t;
    }
    function explicitBlocks(t) {
        const skip = i => {
            while (t[i] && (t[i].k === "comment" || t[i].k === "pp")) i++;
            return i;
        };
        function close(i) {
            const open = t[i] && t[i].v;
            const end = open === "(" ? ")" : open === "{" ? "}" : open === "[" ? "]" : "";
            if (!end) return i;
            let level = 0;
            for (let j = i; j < t.length; j++) {
                if (t[j].v === open) level++;
                if (t[j].v === end && --level === 0) return j;
            }
            return i;
        }
        function statementEnd(i) {
            i = skip(i);
            if (i >= t.length) return t.length - 1;
            const v = t[i].v;
            if (v === "{") return close(i);
            if (["if", "for", "while", "switch"].includes(v) && t[skip(i + 1)]?.v === "(") {
                const c = close(skip(i + 1));
                const end = statementEnd(skip(c + 1));
                if (v === "if" && t[skip(end + 1)]?.v === "else") {
                    return statementEnd(skip(skip(end + 1) + 1));
                }
                return end;
            }
            if (v === "do") {
                const end = statementEnd(skip(i + 1));
                const w = skip(end + 1);
                if (t[w]?.v === "while" && t[skip(w + 1)]?.v === "(") {
                    const c = close(skip(w + 1));
                    if (t[skip(c + 1)]?.v === ";") return skip(c + 1);
                }
                return end;
            }
            for (let j = i; j < t.length; j++) {
                if (["(", "[", "{"].includes(t[j].v)) { j = close(j); continue; }
                if (t[j].v === ";") return j;
                if (t[j].v === "}") return Math.max(i, j - 1);
            }
            return t.length - 1;
        }
        const open = new Map(), done = new Map();
        const put = (start, end) => {
            open.set(start, (open.get(start) || 0) + 1);
            done.set(end + 1, (done.get(end + 1) || 0) + 1);
        };
        for (let i = 0; i < t.length; i++) {
            const v = t[i].v;
            if (["if", "for", "while"].includes(v) && t[skip(i + 1)]?.v === "(") {
                const endCond = close(skip(i + 1));
                const first = skip(endCond + 1);
                if (first < t.length && t[first].v !== "{" && t[first].v !== ";") {
                    put(first, statementEnd(first));
                }
            }
            if (v === "else") {
                const first = skip(i + 1);
                if (t[first]?.v !== "{" && t[first]?.v !== "if") put(first, statementEnd(first));
            }
        }
        const result = [];
        for (let i = 0; i <= t.length; i++) {
            for (let j = 0; j < (done.get(i) || 0); j++) result.push({v: "}", k: "added"});
            for (let j = 0; j < (open.get(i) || 0); j++) result.push({v: "{", k: "added"});
            if (i < t.length) result.push(t[i]);
        }
        return result;
    }
    function arrange(t) {
        const output = [];
        let line = "", depth = 0, currentDepth = 0, paren = 0;
        let previous = "", earlier = "", isCase = false;
        const blocks = [];
        function flush() {
            if (line.trim()) output.push("    ".repeat(Math.max(0, currentDepth)) + line.trimEnd());
            line = "";
            currentDepth = depth;
        }
        function emit(v) {
            let space = false;
            const a = previous, b = earlier;
            if (line.length) {
                if ([";", ",", ")", "]", ".", "->", "++", "--"].includes(v)) space = false;
                else if (v === "(") space = ["if", "for", "while", "switch", ","].includes(a);
                else if (v === "[" || ["(", "[", ".", "->"].includes(a)) space = false;
                else if (v === ":" && isCase) space = false;
                else if ([",", ":", "?", ";"].includes(a)) space = true;
                else if (a === "!" || a === "~") space = false;
                else if (v === "*") space = true;
                else if (a === "*" && (/_t$/.test(b) || ["char", "int", "bool", "void", "const", "float", "double", "{", "return", "=", "("].includes(b))) space = false;
                else if (v === "-" && ["(", "[", "return", ",", ":", "?"].includes(a)) space = false;
                else if (v === "-" && a === "=") space = true;
                else if (a === "-" && ["(", "[", "=", ",", ":", "?"].includes(b)) space = false;
                else if (v === "&" && ["(", "=", "return", ","].includes(a)) space = false;
                else if (a === "&" && ["(", "=", "return", ","].includes(b)) space = false;
                else space = true;
            }
            line += (space ? " " : "") + v;
            earlier = previous;
            previous = v;
        }
        for (let i = 0; i < t.length; i++) {
            const item = t[i], v = item.v, next = t[i + 1]?.v;
            if (item.k === "pp") {
                flush();
                output.push(v.trim());
                previous = earlier = "";
                continue;
            }
            if (item.k === "comment") {
                flush();
                v.split("\n").forEach((part, index) => {
                    let txt = part.trim();
                    if (index > 0) txt = txt.replace(/^\*\s?/, "* ");
                    output.push("    ".repeat(depth) + txt);
                });
                previous = earlier = "";
                continue;
            }
            if (v === "(") { emit(v); paren++; continue; }
            if (v === ")") { emit(v); paren--; continue; }
            if (v === "{") {
                const parent = blocks.at(-1);
                const initializer = ["init", "inline"].includes(parent?.kind) || ["=", ",", "{"].includes(previous);
                let small = false;
                if (initializer) {
                    let count = 0, level = 0;
                    for (let j = i + 1; j < t.length; j++) {
                        if (t[j].v === "{") level++;
                        if (t[j].v === "}") {
                            if (level === 0) { small = count <= 5; break; }
                            level--;
                        }
                        if (level === 0) count++;
                    }
                }
                if (initializer && (["init", "inline"].includes(parent?.kind) || small)) {
                    emit("{");
                    blocks.push({kind: "inline"});
                } else if (initializer) {
                    emit("{");
                    flush();
                    blocks.push({kind: "init"});
                    depth++;
                    currentDepth = depth;
                } else {
                    flush();
                    output.push("    ".repeat(depth) + "{");
                    blocks.push({kind: "block"});
                    depth++;
                    currentDepth = depth;
                }
                previous = "{";
                earlier = "";
                continue;
            }
            if (v === "}") {
                const ctx = blocks.pop() || {kind: "block"};
                if (ctx.kind === "inline") { emit("}"); continue; }
                flush();
                depth = Math.max(0, depth - 1);
                currentDepth = depth;
                emit("}");
                if (![";", ",", ")"].includes(next)) {
                    flush();
                    if (depth === 0) output.push("");
                }
                continue;
            }
            if (v === ";" && paren === 0) {
                emit(";");
                flush();
                previous = earlier = "";
                continue;
            }
            if (v === ",") {
                emit(",");
                if (blocks.at(-1)?.kind === "init" && paren === 0) {
                    flush(); previous = earlier = "";
                } else if (line.length > 114) {
                    flush(); currentDepth = depth + 1;
                }
                continue;
            }
            if (["case", "default"].includes(v) && paren === 0 && blocks.at(-1)?.kind === "block") {
                flush();
                currentDepth = Math.max(0, depth - 1);
                emit(v);
                isCase = true;
                continue;
            }
            if (v === ":" && isCase) {
                emit(v);
                flush();
                previous = earlier = "";
                isCase = false;
                continue;
            }
            if (line.length > 114 && ["&&", "||"].includes(v)) {
                flush();
                currentDepth = depth + 1;
            }
            emit(v);
        }
        flush();
        return output.join("\n").replace(/\n{3,}/g, "\n\n").replace(/}\n\n([A-Za-z_]\w*;)/g, "} $1").trimEnd() + "\n";
    }
    const moduleInfo = {
        ad9833: "Pilote SPI du generateur DDS : frequence et RESET.",
        analogique: "Profils de calibration et calcul des codes de gain et d'offset.",
        eeprom: "Communication I2C et acces a la memoire 24LC32.",
        encodeur: "Interruption et acquisition de l'encodeur rotatif.",
        entree: "Decodage de quadrature et anti-rebond du bouton.",
        epot: "Commande et verification des potentiometres MCP45HV51.",
        gf: "Codes de resultat communs et calcul CRC32.",
        i2c_commun: "Configuration du bus I2C partage entre les pilotes.",
        initialisation: "Demarrage des sous-systemes de l'ESP32-C6.",
        integration: "Coordination des etats, des sorties et de la sauvegarde.",
        integration_esp32: "Adaptation des pilotes ESP-IDF au moteur d'integration.",
        main: "Point d'entree et supervision periodique.",
        menu: "Navigation OLED a trois lignes et choix des signaux.",
        oled: "Police, tampon graphique et transfert SH1106.",
        signaux: "Catalogue des formes d'ondes, limites et formats.",
        sorties_digitales: "Pilotage de deux sorties numeriques avec LEDC.",
        stockage: "Sauvegarde EEPROM sur deux banques et controle CRC.",
        usb_protocol: "Decodeur et encodeur du protocole ASCII USB.",
        usb_service: "Traitement des commandes et mise en file des reponses.",
        usb_transport: "Transport de trames sur la liaison USB."
    };
    const docs = {
        menu: {
            liste: "Affiche trois lignes du catalogue et le defilement du choix.",
            valider: "Selectionne un signal, puis n'active la sortie qu'en cas de succes.",
            menu_initialiser: "Reinitialise la page, les indices et les temporisations.",
            menu_traiter: "Interprete les rotations, les clics et les appuis longs.",
            afficher: "Dessine l'ecran suivant l'etat reel du systeme.",
            menu_periodique: "Planifie le rafraichissement et l'animation OLED."
        },
        integration: {
            etat_arrete: "Deduit l'etat de repos selon le DDS et la calibration.",
            changer: "Marque les modifications RAM et programme la sauvegarde.",
            integration_arreter: "Arrete toutes les sorties et signale les defauts.",
            integration_initialiser_avec: "Initialise les pilotes et charge l'EEPROM.",
            integration_periodique: "Deroule la sauvegarde sans bloquer le programme.",
            integration_choisir: "Arrete si necessaire avant la selection d'un signal.",
            integration_activer: "Programme le DDS, le gain et l'offset avant activation.",
            integration_modifier: "Valide et applique une edition de signal en RAM.",
            integration_charger: "Recharge la configuration persistante."
        },
        ad9833: {
            Ad9833_Init: "Configure le bus SPI et la mise en securite du DDS.",
            Ad9833_SetSignal: "Charge les registres de frequence et de forme d'onde.",
            Ad9833_EnableOutput: "Controle RESET sans promettre une VOUT nulle."
        },
        analogique: {
            calibration_valider: "Verifie la coherence des points d'etalonnage.",
            analogique_convertir: "Interpole les mesures pour trouver les codes EPOT.",
            analogique_convertir_nominal: "Calcule une estimation non mesuree sur VOUT."
        },
        signaux: {
            signaux_initialiser: "Charge les dix profils fournis dans le cahier des charges.",
            signaux_valide: "Controle les limites de chaque parametre d'un signal.",
            signaux_variation: "Modifie une valeur tout en respectant ses limites."
        },
        stockage: {
            stockage_charger: "Selectionne la banque EEPROM valide la plus recente.",
            stockage_commencer: "Prepare une sauvegarde dans la banque inactive.",
            stockage_pas: "Execute les etapes d'ecriture et de verification."
        },
        usb_protocol: {
            usb_proto_decoder: "Valide une trame et extrait les champs de commande.",
            usb_proto_formater: "Construit une trame ASCII de signal."
        },
        usb_service: {
            executer: "Distribue une commande au sous-systeme concerne.",
            usb_service_octet: "Reconstruit une trame depuis les octets recus.",
            usb_service_periodique: "Surveille les delais de reception et emission."
        },
        oled: {
            oled_initialiser: "Detecte puis initialise le controleur SH1106.",
            oled_texte_grand: "Dessine les glyphes agrandis du menu trois lignes.",
            oled_periodique: "Envoie au plus une page modifiee par iteration."
        },
        eeprom: { eeprom_initialiser: "Verifie l'adresse de la memoire I2C." },
        encodeur: { encodeur_initialiser: "Configure les broches et les interruptions." },
        entree: { quadrature_traiter: "Determine le sens de rotation avec la table des transitions." },
        epot: { Epot_Init: "Associe les deux adresses I2C aux fonctions gain et offset." },
        sorties_digitales: { digitales_commander: "Configure ou arrete un canal PWM LEDC." },
        main: { app_main: "Initialise les services et execute la boucle principale." }
    };
    function document(s, module) {
        const lines = s.trimEnd().split("\n");
        const output = [
            "/*==================================================================================================",
            " * CFPT - Equipe 1 - Projet generateur de fonctions",
            " * MODULE     : " + module.toUpperCase() + ".C",
            " * DESCRIPTIF : " + moduleInfo[module],
            " *==================================================================================================*/"
        ];
        let lastInclude = -1;
        for (let i = 0; i < lines.length; i++) if (lines[i].startsWith("#include ")) lastInclude = i;
        let firstFunction = false, publicSection = false;
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i], next = lines[i + 1] || "";
            const match = !line.startsWith(" ") && next === "{" ? line.match(/([A-Za-z_]\w*)\s*\([^)]*\)$/) : null;
            if (match) {
                if (!firstFunction) {
                    output.push("/*==================================== FONCTIONS ================================================*/");
                    firstFunction = true;
                }
                if (!publicSection && !line.startsWith("static ")) {
                    output.push("/*================================ INTERFACE PUBLIQUE ============================================*/");
                    publicSection = true;
                }
                const description = docs[module]?.[match[1]];
                if (description) {
                    output.push("/*--------------------------------------------------------------------------------------------------");
                    output.push(" * Fonction   : " + match[1] + "()");
                    output.push(" * Descriptif : " + description);
                    output.push(" *--------------------------------------------------------------------------------------------------*/");
                }
            }
            output.push(line);
            if (i === lastInclude) {
                output.push("");
                output.push("/*============================== VARIABLES ET CONFIGURATION =======================================*/");
            }
        }
        return output.join("\n").replace(/\n{3,}/g, "\n\n").trimEnd() + "\n";
    }
    function ternaryCount(s) {
        return lex(s).filter(x => x.k === "sym" && x.v === "?").length;
    }
    function formatFile(content, module, replaceTernaries) {
        const updated = replaceTernaries ? replaceTernaries(content, module) : content;
        const tokens = explicitBlocks(lex(updated));
        const code = arrange(tokens);
        const a = tokens.filter(x => x.k !== "comment" && x.k !== "pp").map(x => x.v);
        const b = lex(code).filter(x => x.k !== "comment" && x.k !== "pp").map(x => x.v);
        if (JSON.stringify(a) !== JSON.stringify(b)) {
            const index = a.findIndex((v, i) => v !== b[i]);
            throw Error("Reconstruction de tokens incorrecte pour " + module + " vers " + index);
        }
        return {content: document(code, module), ternaries: ternaryCount(code)};
    }
    return {lex, formatFile, ternaryCount};
})();
if (typeof require !== "undefined" && typeof module !== "undefined" && require.main === module) {
    console.log("Outil de transformation disponible en tant que bibliotheque de refactorisation.");
}
readability;
