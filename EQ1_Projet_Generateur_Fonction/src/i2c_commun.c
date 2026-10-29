#include "i2c_commun.h"
#include "hardware_config.h"

static i2c_master_bus_handle_t bus_partage = NULL;

esp_err_t i2c_commun_initialiser(void)
{
    if (bus_partage != NULL)
    {
        return ESP_OK;
    }

    const i2c_master_bus_config_t config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = SYSTEM_I2C_PORT,
        .sda_io_num = SYSTEM_I2C_SDA_GPIO,
        .scl_io_num = SYSTEM_I2C_SCL_GPIO,
        .glitch_ignore_cnt = SYSTEM_I2C_GLITCH_IGNORE_COUNT,
        .flags.enable_internal_pullup = SYSTEM_I2C_ENABLE_INTERNAL_PULLUPS
    };

    return i2c_new_master_bus(&config, &bus_partage);
}

esp_err_t i2c_commun_desinitialiser(void)
{
    if (bus_partage == NULL)
    {
        return ESP_OK;
    }

    const esp_err_t err = i2c_del_master_bus(bus_partage);
    if (err == ESP_OK)
    {
        bus_partage = NULL;
    }
    return err;
}

i2c_master_bus_handle_t i2c_commun_bus(void)
{
    return bus_partage;
}

bool i2c_commun_est_initialise(void)
{
    return bus_partage != NULL;
}

bool i2c_commun_adresse_presente(uint8_t adresse, int timeout_ms)
{
    if (bus_partage == NULL)
    {
        return false;
    }

    if (timeout_ms <= 0)
    {
        timeout_ms = SYSTEM_I2C_DIAG_PROBE_TIMEOUT_MS;
    }

    return i2c_master_probe(bus_partage, adresse, timeout_ms) == ESP_OK;
}

bool i2c_commun_trouver_adresse(uint8_t adresse_min,
                                uint8_t adresse_max,
                                int timeout_ms,
                                uint8_t *adresse_trouvee)
{
    if ((bus_partage == NULL) ||
        (adresse_trouvee == NULL) ||
        (adresse_min > adresse_max))
    {
        return false;
    }

    for (uint16_t adresse = adresse_min; adresse <= adresse_max; ++adresse)
    {
        if (i2c_commun_adresse_presente((uint8_t)adresse, timeout_ms))
        {
            *adresse_trouvee = (uint8_t)adresse;
            return true;
        }
    }

    return false;
}
