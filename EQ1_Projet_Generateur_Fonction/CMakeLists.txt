# ==============================================================================================
# CFPT - Equipe 1 - Generateur de fonctions
# Racine CMake du firmware ESP-IDF / ESP32-C6.
# ==============================================================================================

cmake_minimum_required(VERSION 3.16.0)

# Charge la configuration et les outils de compilation fournis par ESP-IDF.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Nom du projet embarque (conserve a l'identique).
project(EQ1_Projet_Generateur_Fonction)
