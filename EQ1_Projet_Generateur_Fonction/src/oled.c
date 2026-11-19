/*==================================================================================================
 * CFPT - Equipe 1 - Projet generateur de fonctions
 * MODULE     : OLED.C
 * DESCRIPTIF : Police, tampon graphique et transfert SH1106.
 *==================================================================================================*/
/* SH1106 et police du prototype. Transfert d'une page sale par iteration. */
#include "oled.h"
#include "i2c_commun.h"
#include "hardware_config.h"
#include <stddef.h>
#include <string.h>
#include "driver/i2c_master.h"

/*============================== VARIABLES ET CONFIGURATION =======================================*/
/* Dimensions physiques, police 5x7 et transfert de pages SH1106. */
#define OLED_PIXELS_PAR_OCTET           8
#define OLED_NOMBRE_PAGES              (OLED_HAUTEUR_PX / OLED_PIXELS_PAR_OCTET)
#define OLED_POLICE_COLONNES           5
#define OLED_POLICE_PAS_X_PX           6
#define OLED_POLICE_LIGNE_COLONNES     21
#define OLED_POLICE_HAUTEUR_PX         7
#define OLED_POLICE_AGRAND_X_NUM       3
#define OLED_POLICE_AGRAND_X_DEN       2
#define OLED_POLICE_AGRAND_Y          2
#define OLED_COMMANDE_I2C_MAX_OCTETS  31U
#define OLED_COMMANDE_TAMPON_OCTETS   32U
#define OLED_DATA_I2C_OCTETS           (OLED_LARGEUR_PX + 1U)
#define OLED_COMMUNICATION_TIMEOUT_MS 30
#define OLED_NOUVEL_ESSAI_MS          2000U
#define OLED_PAGES_A_REENVOYER         UINT8_MAX
#define OLED_I2C_PREFIXE_DONNEES      0x40U
#define OLED_SH1106_SELECT_PAGE       0xB0U
#define OLED_SH1106_OFFSET_COLONNE    0x02U
#define OLED_SH1106_COLONNE_HAUTE     0x10U

static uint8_t image_voulue[OLED_NOMBRE_PAGES][OLED_LARGEUR_PX], image_transmise[OLED_NOMBRE_PAGES][OLED_LARGEUR_PX];
static uint8_t pages_a_renvoyer = OLED_PAGES_A_REENVOYER, page_suivante;
static uint32_t erreurs_transmission, dernier_essai_ms;
static bool ecran_present;
static i2c_master_dev_handle_t dispositif_oled;
typedef struct
{
    char caractere;
    uint8_t colonnes[OLED_POLICE_COLONNES];
} glyph_t;
static const glyph_t police_embarquee[] = {
    { ' ', { 0x00, 0x00, 0x00, 0x00, 0x00 } },
    { '!', { 0x00, 0x00, 0x5f, 0x00, 0x00 } },
    { '$', { 0x24, 0x2a, 0x7f, 0x2a, 0x12 } },
    { '#', { 0x14, 0x7f, 0x14, 0x7f, 0x14 } },
    { '%', { 0x23, 0x13, 0x08, 0x64, 0x62 } },
    { '+', { 0x08, 0x08, 0x3e, 0x08, 0x08 } },
    { ',', { 0x00, 0x50, 0x30, 0x00, 0x00 } },
    { '-', { 0x08, 0x08, 0x08, 0x08, 0x08 } },
    { '.', { 0x00, 0x60, 0x60, 0x00, 0x00 } },
    { '/', { 0x20, 0x10, 0x08, 0x04, 0x02 } },
    { '0', { 0x3e, 0x51, 0x49, 0x45, 0x3e } },
    { '1', { 0x00, 0x42, 0x7f, 0x40, 0x00 } },
    { '2', { 0x42, 0x61, 0x51, 0x49, 0x46 } },
    { '3', { 0x21, 0x41, 0x45, 0x4b, 0x31 } },
    { '4', { 0x18, 0x14, 0x12, 0x7f, 0x10 } },
    { '5', { 0x27, 0x45, 0x45, 0x45, 0x39 } },
    { '6', { 0x3c, 0x4a, 0x49, 0x49, 0x30 } },
    { '7', { 0x01, 0x71, 0x09, 0x05, 0x03 } },
    { '8', { 0x36, 0x49, 0x49, 0x49, 0x36 } },
    { '9', { 0x06, 0x49, 0x49, 0x29, 0x1e } },
    { ':', { 0x00, 0x36, 0x36, 0x00, 0x00 } },
    { '=', { 0x14, 0x14, 0x14, 0x14, 0x14 } },
    { '>', { 0x41, 0x22, 0x14, 0x08, 0x00 } },
    { '?', { 0x02, 0x01, 0x51, 0x09, 0x06 } },
    { 'A', { 0x7e, 0x11, 0x11, 0x11, 0x7e } },
    { 'B', { 0x7f, 0x49, 0x49, 0x49, 0x36 } },
    { 'C', { 0x3e, 0x41, 0x41, 0x41, 0x22 } },
    { 'D', { 0x7f, 0x41, 0x41, 0x22, 0x1c } },
    { 'E', { 0x7f, 0x49, 0x49, 0x49, 0x41 } },
    { 'F', { 0x7f, 0x09, 0x09, 0x09, 0x01 } },
    { 'G', { 0x3e, 0x41, 0x49, 0x49, 0x7a } },
    { 'H', { 0x7f, 0x08, 0x08, 0x08, 0x7f } },
    { 'I', { 0x00, 0x41, 0x7f, 0x41, 0x00 } },
    { 'J', { 0x20, 0x40, 0x41, 0x3f, 0x01 } },
    { 'K', { 0x7f, 0x08, 0x14, 0x22, 0x41 } },
    { 'L', { 0x7f, 0x40, 0x40, 0x40, 0x40 } },
    { 'M', { 0x7f, 0x02, 0x0c, 0x02, 0x7f } },
    { 'N', { 0x7f, 0x04, 0x08, 0x10, 0x7f } },
    { 'O', { 0x3e, 0x41, 0x41, 0x41, 0x3e } },
    { 'P', { 0x7f, 0x09, 0x09, 0x09, 0x06 } },
    { 'Q', { 0x3e, 0x41, 0x51, 0x21, 0x5e } },
    { 'R', { 0x7f, 0x09, 0x19, 0x29, 0x46 } },
    { 'S', { 0x46, 0x49, 0x49, 0x49, 0x31 } },
    { 'T', { 0x01, 0x01, 0x7f, 0x01, 0x01 } },
    { 'U', { 0x3f, 0x40, 0x40, 0x40, 0x3f } },
    { 'V', { 0x1f, 0x20, 0x40, 0x20, 0x1f } },
    { 'W', { 0x7f, 0x20, 0x18, 0x20, 0x7f } },
    { 'X', { 0x63, 0x14, 0x08, 0x14, 0x63 } },
    { 'Y', { 0x03, 0x04, 0x78, 0x04, 0x03 } },
    { 'Z', { 0x61, 0x51, 0x49, 0x45, 0x43 } },
    { '*', { 0x14, 0x08, 0x3e, 0x08, 0x14 } },
    { '_', { 0x40, 0x40, 0x40, 0x40, 0x40 } }
};
/*==================================== FONCTIONS ================================================*/
static const uint8_t *glyphe(char caractere)
{
    if (caractere >= 'a' && caractere <= 'z')
    {
        caractere = (char)(caractere - ('a' - 'A'));
    }
    for (unsigned index_glyphe = 0; index_glyphe < sizeof(police_embarquee) / sizeof(police_embarquee[0]); index_glyphe++)
    {
        if (police_embarquee[index_glyphe].caractere == caractere)
        {
            return police_embarquee[index_glyphe].colonnes;
        }
    }
    return police_embarquee[0].colonnes;
}

static bool commande(const uint8_t *octets_commande, size_t longueur_commande)
{
    if (!dispositif_oled || longueur_commande > OLED_COMMANDE_I2C_MAX_OCTETS)
    {
        return false;
    }
    uint8_t trame_commande[OLED_COMMANDE_TAMPON_OCTETS] = { 0 };
    memcpy(trame_commande + 1, octets_commande, longueur_commande);
    return i2c_master_transmit(dispositif_oled, trame_commande, longueur_commande + 1, OLED_COMMUNICATION_TIMEOUT_MS) == ESP_OK;
}

/*================================ INTERFACE PUBLIQUE ============================================*/
/*--------------------------------------------------------------------------------------------------
 * Fonction   : oled_initialiser()
 * Descriptif : Detecte puis initialise le controleur SH1106.
 *--------------------------------------------------------------------------------------------------*/
bool oled_initialiser(void)
{
    if (ecran_present)
    {
        return true;
    }
    if (i2c_commun_initialiser() != ESP_OK)
    {
        return false;
    }
    if (!dispositif_oled)
    {
        i2c_master_bus_handle_t bus = i2c_commun_bus();
        uint8_t adresse_i2c = SYSTEM_OLED_I2C_ADDRESS_PRIMARY;
        if (i2c_master_probe(bus, adresse_i2c, OLED_COMMUNICATION_TIMEOUT_MS) != ESP_OK)
        {
            adresse_i2c = SYSTEM_OLED_I2C_ADDRESS_SECONDARY;
            if (i2c_master_probe(bus, adresse_i2c, OLED_COMMUNICATION_TIMEOUT_MS) != ESP_OK)
            {
                return false;
            }
        }
        i2c_device_config_t config_i2c = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = adresse_i2c,
            .scl_speed_hz = SYSTEM_OLED_I2C_SPEED_HZ
        };
        if (i2c_master_bus_add_device(bus, &config_i2c, &dispositif_oled) != ESP_OK)
        {
            dispositif_oled = NULL;
            return false;
        }
    }
    static const uint8_t sequence_initialisation[] = {
        0xAE,
        0xD5,
        0x80,
        0xA8,
        0x3F,
        0xD3,
        0x00,
        0x40,
        0xAD,
        0x8B,
        0xA1,
        0xC8,
        0xDA,
        0x12,
        0x81,
        0x80,
        0xD9,
        0x22,
        0xDB,
        0x35,
        0xA4,
        0xA6,
        0xAF
    };
    ecran_present = commande(sequence_initialisation, sizeof(sequence_initialisation));
    if (ecran_present)
    {
        pages_a_renvoyer = OLED_PAGES_A_REENVOYER;
    }
    else
    {
        erreurs_transmission++;
    }
    return ecran_present;
}

bool oled_est_present(void)
{
    return ecran_present;
}

uint32_t oled_erreurs(void)
{
    return erreurs_transmission;
}

void oled_effacer(void)
{
    memset(image_voulue, 0, sizeof(image_voulue));
}

void oled_texte(int ligne, int colonne, const char *texte)
{
    if (ligne < 0 || ligne >= OLED_NOMBRE_PAGES || colonne < 0 || colonne >= OLED_POLICE_LIGNE_COLONNES || !texte)
    {
        return;
    }
    int pixel_x = colonne * OLED_POLICE_PAS_X_PX;
    while (*texte && pixel_x + OLED_POLICE_COLONNES < OLED_LARGEUR_PX)
    {
        memcpy(&image_voulue[ligne][pixel_x], glyphe(*texte++), OLED_POLICE_COLONNES);
        image_voulue[ligne][pixel_x + OLED_POLICE_COLONNES] = 0;
        pixel_x += OLED_POLICE_PAS_X_PX;
    }
}

void oled_ligne(int ligne, const char *texte, bool inverser)
{
    if (ligne < 0 || ligne >= OLED_NOMBRE_PAGES)
    {
        return;
    }
    memset(image_voulue[ligne], 0, OLED_LARGEUR_PX);
    oled_texte(ligne, 0, texte);
    if (inverser)
    {
        for (unsigned x = 0; x < OLED_LARGEUR_PX; x++)
        {
            image_voulue[ligne][x] ^= UINT8_MAX;
        }
    }
}

/* Police de la branche menu-oled-3-lignes : colonnes reparties a 1.5x,
* lignes doublees. Un glyphe occupe 7 pixels dans une cellule de 9 pixels.
* Coordonnees bornees avant les additions, meme pour INT_MIN / INT_MAX. */
/*--------------------------------------------------------------------------------------------------
 * Fonction   : oled_texte_grand()
 * Descriptif : Dessine les glyphes agrandis du menu trois lignes.
 *--------------------------------------------------------------------------------------------------*/
void oled_texte_grand(int x, int y, const char *texte)
{
    if (!texte || x < - 7 || x >= OLED_LARGEUR_PX || y <= - OLED_GRAND_HAUTEUR || y >= OLED_HAUTEUR_PX)
    {
        return;
    }
    while (*texte && x <= 120)
    {
        const uint8_t *colonnes_glyphe = glyphe(*texte++);
        for (int colonne_glyphe = 0; colonne_glyphe < OLED_POLICE_COLONNES; colonne_glyphe++)
        {
            int pixel_debut =(colonne_glyphe * OLED_POLICE_AGRAND_X_NUM) / OLED_POLICE_AGRAND_X_DEN, pixel_fin =((colonne_glyphe + 1) * OLED_POLICE_AGRAND_X_NUM) / OLED_POLICE_AGRAND_X_DEN;
            for (int ligne_glyphe = 0; ligne_glyphe < OLED_POLICE_HAUTEUR_PX; ligne_glyphe++)
            {
                if (!(colonnes_glyphe[colonne_glyphe] &(1U << ligne_glyphe)))
                {
                    continue;
                }
                for (int dx = pixel_debut; dx < pixel_fin; dx++)
                {
                    for (int dy = 0; dy < OLED_POLICE_AGRAND_Y; dy++)
                    {
                        oled_pixel(x + dx, y + ligne_glyphe * OLED_POLICE_AGRAND_Y + dy);
                    }
                }
            }
        }
        x += OLED_GRAND_PAS_X;
    }
}

void oled_inverser_zone(int x, int y, int largeur, int hauteur)
{
    if (largeur <= 0 || hauteur <= 0)
    {
        return;
    }
    /* Sommes sur 64 bits, puis intersection avec la dalle : travail borne. */
    int64_t droite =(int64_t) x + largeur, bas =(int64_t) y + hauteur;
    if (x >= OLED_LARGEUR_PX || y >= OLED_HAUTEUR_PX || droite <= 0 || bas <= 0)
    {
        return;
    }
    int x0 = x, y0 = y;
    if (x < 0)
    {
        x0 = 0;
    }
    if (y < 0)
    {
        y0 = 0;
    }
    int x1 =(int) droite, y1 =(int) bas;
    if (droite > OLED_LARGEUR_PX)
    {
        x1 = OLED_LARGEUR_PX;
    }
    if (bas > OLED_HAUTEUR_PX)
    {
        y1 = OLED_HAUTEUR_PX;
    }
    for (int py = y0; py < y1; py++)
    {
        for (int px = x0; px < x1; px++)
        {
            image_voulue[py / OLED_PIXELS_PAR_OCTET][px] ^=(uint8_t)(1U <<(py % OLED_PIXELS_PAR_OCTET));
        }
    }
}

void oled_pixel(int x, int y)
{
    if (x >= 0 && x < OLED_LARGEUR_PX && y >= 0 && y < OLED_HAUTEUR_PX)
    {
        image_voulue[y / OLED_PIXELS_PAR_OCTET][x] |=(uint8_t)(1U <<(y % OLED_PIXELS_PAR_OCTET));
    }
}

void oled_segment(int x, int y, int x1, int y1)
{
    /* Coordonnees limitees pour borner le travail du traceur. */
    if (x < 0 || x > OLED_LARGEUR_PX - 1 || x1 < 0 || x1 > OLED_LARGEUR_PX - 1 || y < 0 || y > OLED_HAUTEUR_PX - 1 || y1 < 0 || y1 > OLED_HAUTEUR_PX - 1)
    {
        return;
    }
    int dx = x1 - x;
    if (x1 <= x)
    {
        dx = x - x1;
    }
    int dy = y1 - y;
    if (y1 <= y)
    {
        dy = y - y1;
    }
    int sx = -1;
    if (x < x1)
    {
        sx = 1;
    }
    int sy = -1;
    if (y < y1)
    {
        sy = 1;
    }
    int erreur_bresenham = dx - dy;
    for (;;)
    {
        oled_pixel(x, y);
        if (x == x1 && y == y1)
        {
            break;
        }
        int erreur_doublee = 2 * erreur_bresenham;
        if (erreur_doublee > - dy)
        {
            erreur_bresenham -= dy;
            x += sx;
        }
        if (erreur_doublee < dx)
        {
            erreur_bresenham += dx;
            y += sy;
        }
    }
}

/*--------------------------------------------------------------------------------------------------
 * Fonction   : oled_periodique()
 * Descriptif : Envoie au plus une page modifiee par iteration.
 *--------------------------------------------------------------------------------------------------*/
void oled_periodique(uint32_t maintenant_ms)
{
    if (!ecran_present)
    {
        if ((uint32_t)(maintenant_ms - dernier_essai_ms) >= OLED_NOUVEL_ESSAI_MS)
        {
            dernier_essai_ms = maintenant_ms;
            (void) oled_initialiser();
        }
        return;
    }
    for (unsigned passage = 0; passage < OLED_NOMBRE_PAGES; passage++)
    {
        unsigned index_page = page_suivante;
        page_suivante =(uint8_t)((page_suivante + 1) % OLED_PIXELS_PAR_OCTET);
        if (!(pages_a_renvoyer &(1U << index_page)) && memcmp(image_voulue[index_page], image_transmise[index_page], OLED_LARGEUR_PX) == 0)
        {
            continue;
        }
        uint8_t commandes_page[] = {
            (uint8_t)(OLED_SH1106_SELECT_PAGE | index_page),
            OLED_SH1106_OFFSET_COLONNE,
            OLED_SH1106_COLONNE_HAUTE
        }, trame_image[OLED_DATA_I2C_OCTETS];
        trame_image[0] = OLED_I2C_PREFIXE_DONNEES;
        memcpy(trame_image + 1, image_voulue[index_page], OLED_LARGEUR_PX);
        if (!commande(commandes_page, sizeof(commandes_page)) || i2c_master_transmit(dispositif_oled, trame_image, sizeof(trame_image), OLED_COMMUNICATION_TIMEOUT_MS) != ESP_OK)
        {
            ecran_present = false;
            erreurs_transmission++;
            dernier_essai_ms = maintenant_ms;
            return;
        }
        memcpy(image_transmise[index_page], image_voulue[index_page], OLED_LARGEUR_PX);
        pages_a_renvoyer &=(uint8_t) ~(1U << index_page);
        return;
    }
}
