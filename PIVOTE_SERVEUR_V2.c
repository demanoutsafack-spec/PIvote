/**
 * @file serveur_main.c
 * @brief Point d'entree du SERVEUR PIVOTE V2 (administrateur).
 *
 * Flux d'execution :
 *   1. auth_init           -> initialise le fichier users.csv
 *   2. ecranConnexionAdmin -> cree/authentifie l'administrateur
 *   3. chargerDonnees      -> recharge les donnees de vote persistees
 *   4. menuServeur         -> boucle principale du menu admin
 *
 * Compilation (MinGW / Code::Blocks, C99) :
 * gcc -std=c99 -Wall serveur_impl.c serveur_main.c auth.c -o serveur.exe -lws2_32
 */

#include "serveur.h"
#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <winsock2.h>
#include <windows.h>
#include "auth.h"

int main(void)
{
    /* 1252 = Windows Latin-1 : support complet des accents           */
    SetConsoleOutputCP(1252);
    SetConsoleCP(1252);
    setlocale(LC_ALL, "");
    AuthStatus st = auth_init(CSV_PATH);
    if (st != AUTH_OK)
    {
        printf("Erreur d'initialisation du fichier utilisateurs (code=%d).\n", st);
        return 1;
    }

    if (!ecranConnexionAdmin())
    {
        printf("Impossible de se connecter. Fermeture.\n");
        system("pause");
        return 1;
    }

    chargerDonnees();
    menuServeur();

    return 0;
}
