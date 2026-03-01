/**
 * @file serveur_impl.c
 * @brief Implementation de toutes les fonctions du SERVEUR PIVOTE V2.
 *
 * Nouveautes V2 :
 *   - afficherBarresASCII() : resultats visuels en barres dans le terminal
 *   - afficherGagnant()     : proclamation automatique du gagnant
 *   - genererRapportFinal() : fichier rapport_final.txt complet
 *   - fermerVote()          : appelle les 3 fonctions ci-dessus automatiquement
 *
 * Compilation (MinGW / Code::Blocks, C99) :
 * gcc -std=c99 -Wall serveur_impl.c serveur_main.c auth.c -o serveur.exe -lws2_32
 */

#include <conio.h>
#include "serveur.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <winsock2.h>
#include <windows.h>
#include "auth.h"
#include <locale.h>

/* =========================================================
 * COULEURS CONSOLE
 * ========================================================= */
#define COULEUR_NORMAL   7   /* Blanc sur fond noir  */
#define COULEUR_SELEC   11   /* Cyan clair sur fond noir */
#define COULEUR_TITRE    3   /* Cyan foncé */

static void setCouleur(int couleur)
{
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), couleur);
}

#pragma comment(lib, "ws2_32.lib")

/* =========================================================
 * VARIABLES GLOBALES
 * ========================================================= */
Electeur electeurs[MAX];
Candidat candidats[MAX];
int nbElecteurs        = 0;
int nbCandidats        = 0;
int voteOuvert         = 0;
int affichageAutoActif = 0;

static AuthUser adminConnecte;

/* =========================================================
 * 1. HELPERS CONSOLE
 * ========================================================= */
void lire_ligne_srv(const char *invite, char *buf, size_t taille)
{
    printf("%s", invite);
    if (fgets(buf, (int)taille, stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[len-1] = '\0';
    }
}

void vider_buffer_stdin(void)
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

static void afficher_utilisateur(const AuthUser *u)
{
    printf(" - %s (role=%s, actif=%s)\n",
           u->username, u->role, u->active ? "oui" : "non");
}

/* =========================================================
 * 2. GESTION DES ELECTEURS ET CANDIDATS
 * ========================================================= */
void ajouterElecteur(void)
{
    if (nbElecteurs >= MAX) {
        printf("Nombre maximum d'electeurs atteint.\n");
        return;
    }

    Electeur e;
    char username[AUTH_MAX_USERNAME + 1];
    char password[AUTH_MAX_PASSWORD + 1];

    printf("ID numérique de l'électeur : ");
    scanf("%d", &e.id);
    vider_buffer_stdin();

    printf("Nom de l'electeur : ");
    fgets(e.nom, sizeof(e.nom), stdin);
    {
        size_t l = strlen(e.nom);
        if (l > 0 && e.nom[l-1] == '\n') e.nom[l-1] = '\0';
    }

    lire_ligne_srv("Identifiant de connexion (login) : ", username, sizeof(username));
    lire_ligne_srv("Mot de passe initial             : ", password, sizeof(password));

    for (int i = 0; i < nbElecteurs; i++) {
        if (electeurs[i].id == e.id) {
            printf("Erreur : un electeur avec l'ID %d existe deja.\n", e.id);
            return;
        }
    }

    AuthStatus st = auth_register_user(CSV_PATH, username, password, "votant");
    if (st == AUTH_ERR_EXISTS) {
        printf("Erreur : un compte '%s' existe deja.\n", username);
        return;
    } else if (st != AUTH_OK) {
        printf("Erreur creation du compte (code=%d).\n", st);
        return;
    }

    e.a_vote     = 0;
    e.vote_blanc = 0;
    strncpy(e.username, username, AUTH_MAX_USERNAME);
    e.username[AUTH_MAX_USERNAME] = '\0';

    electeurs[nbElecteurs++] = e;
    printf("Electeur '%s' (login: %s) enregistré avec succes.\n", e.nom, username);
}

void afficherElecteurs(void)
{
    for (int i = 0; i < nbElecteurs; i++)
        printf("ID:%d | %s (login:%s) | A vote: %s\n",
               electeurs[i].id, electeurs[i].nom,
               electeurs[i].username,
               electeurs[i].a_vote ? "OUI" : "NON");
}

void ajouterCandidat(void)
{
    if (nbCandidats >= MAX) return;
    Candidat c;
    printf("ID : ");
    scanf("%d", &c.id);
    vider_buffer_stdin();
    printf("Nom : ");
    fgets(c.nom, sizeof(c.nom), stdin);
    {
        size_t l = strlen(c.nom);
        if (l > 0 && c.nom[l-1] == '\n') c.nom[l-1] = '\0';
    }
    c.voix = 0;
    candidats[nbCandidats++] = c;
    printf("Candidat ajouté.\n");
}

void afficherCandidats(void)
{
    for (int i = 0; i < nbCandidats; i++)
        printf("ID:%d | %s | Voix: %d\n",
               candidats[i].id, candidats[i].nom, candidats[i].voix);
}

/* =========================================================
 * 3. LOGIQUE DE VOTE
 * ========================================================= */
void ouvrirVote(void)
{
    voteOuvert = 1;
    printf("Vote OUVERT.\n");
}

/*
 * fermerVote() :
 * Ferme le scrutin puis enchaine automatiquement :
 *   1. Affichage des barres ASCII
 *   2. Proclamation du gagnant
 *   3. Generation du rapport final TXT
 */
void fermerVote(void)
{
    voteOuvert = 0;
    printf("Vote FERME.\n\n");

    printf("========================================\n");
    printf("   RESULTATS FINAUX DU SCRUTIN\n");
    printf("========================================\n");
    afficherBarresASCII();

    printf("\n");
    afficherGagnant();

    printf("\n");
    genererRapportFinal();
}

void afficherResultats(void)
{
    for (int i = 0; i < nbCandidats; i++)
        printf("%s : %d voix\n", candidats[i].nom, candidats[i].voix);
}

void afficherStatistiques(void)
{
    int v = 0, b = 0;
    for (int i = 0; i < nbElecteurs; i++) {
        if (electeurs[i].a_vote) {
            v++;
            if (electeurs[i].vote_blanc) b++;
        }
    }
    printf("Votants: %d / %d | Votes blancs: %d\n", v, nbElecteurs, b);
}
/* =========================================================
 * 4. NOUVELLES FONCTIONNALITES
 * ========================================================= */

/*
 * afficherBarresASCII()
 * --------------------
 * Affiche chaque candidat avec une barre de 20 caracteres '#'
 * proportionnelle a son score, suivie du nombre de voix et
 * du pourcentage.
 *
 * Exemple :
 *   Alice   [################    ] 16 voix (80.0%)
 *   Bob     [####                ]  4 voix (20.0%)
 *   BLANC   [                    ]  0 voix  (0.0%)
 */
void afficherBarresASCII(void)
{
    if (nbCandidats == 0) {
        printf("Aucun candidat enregistré.\n");
        return;
    }

    /* Calcul du total des voix (candidats + blancs) */
    int totalVoix = 0;
    for (int i = 0; i < nbCandidats; i++)
        totalVoix += candidats[i].voix;

    int blancs = 0;
    for (int i = 0; i < nbElecteurs; i++)
        if (electeurs[i].vote_blanc) blancs++;
    totalVoix += blancs;

    /* Largeur de la barre */
    int largeur = 20;

    printf("\n");
    for (int i = 0; i < nbCandidats; i++) {
        double pct    = (totalVoix > 0) ? (100.0 * candidats[i].voix / totalVoix) : 0.0;
        int    rempli = (totalVoix > 0) ? (largeur * candidats[i].voix / totalVoix) : 0;

        printf("  %-15s [", candidats[i].nom);
        for (int k = 0; k < largeur; k++)
            printf("%c", k < rempli ? '#' : ' ');
        printf("] %3d voix (%5.1f%%)\n", candidats[i].voix, pct);
    }

    /* Ligne votes blancs */
    double pctBlanc  = (totalVoix > 0) ? (100.0 * blancs / totalVoix) : 0.0;
    int    rempliBlanc = (totalVoix > 0) ? (largeur * blancs / totalVoix) : 0;
    printf("  %-15s [", "VOTE BLANC");
    for (int k = 0; k < largeur; k++)
        printf("%c", k < rempliBlanc ? '#' : ' ');
    printf("] %3d voix (%5.1f%%)\n", blancs, pctBlanc);

    printf("\n  Total votes exprimés : %d\n", totalVoix);
}

/* =========================================================
 * 4. NOUVELLES FONCTIONNALITES
 * ========================================================= */

/*
 * afficherBarresASCII()
 * --------------------
 * Affiche chaque candidat avec une barre de 20 caracteres '#'
 * proportionnelle a son score, suivie du nombre de voix et
 * du pourcentage.
 *
 * Exemple :
 *   Alice   [################    ] 16 voix (80.0%)
 *   Bob     [####                ]  4 voix (20.0%)
 *   BLANC   [                    ]  0 voix  (0.0%)
 */
void afficherBarresASCII(void)
{
    if (nbCandidats == 0) {
        printf("Aucun candidat enregistre.\n");
        return;
    }

    /* Calcul du total des voix (candidats + blancs) */
    int totalVoix = 0;
    for (int i = 0; i < nbCandidats; i++)
        totalVoix += candidats[i].voix;

    int blancs = 0;
    for (int i = 0; i < nbElecteurs; i++)
        if (electeurs[i].vote_blanc) blancs++;
    totalVoix += blancs;

    /* Largeur de la barre */
    int largeur = 20;

    printf("\n");
    for (int i = 0; i < nbCandidats; i++) {
        double pct    = (totalVoix > 0) ? (100.0 * candidats[i].voix / totalVoix) : 0.0;
        int    rempli = (totalVoix > 0) ? (largeur * candidats[i].voix / totalVoix) : 0;

        printf("  %-15s [", candidats[i].nom);
        for (int k = 0; k < largeur; k++)
            printf("%c", k < rempli ? '#' : ' ');
        printf("] %3d voix (%5.1f%%)\n", candidats[i].voix, pct);
    }

    /* Ligne votes blancs */
    double pctBlanc  = (totalVoix > 0) ? (100.0 * blancs / totalVoix) : 0.0;
    int    rempliBlanc = (totalVoix > 0) ? (largeur * blancs / totalVoix) : 0;
    printf("  %-15s [", "VOTE BLANC");
    for (int k = 0; k < largeur; k++)
        printf("%c", k < rempliBlanc ? '#' : ' ');
    printf("] %3d voix (%5.1f%%)\n", blancs, pctBlanc);

    printf("\n  Total votes exprimes : %d\n", totalVoix);
}

/*
 * afficherGagnant()
 * -----------------
 * Parcourt les candidats pour trouver le maximum de voix.
 * Si plusieurs candidats sont a egalite, tous sont affiches.
 * Les votes blancs ne peuvent pas gagner.
 */
void afficherGagnant(void)
{
    if (nbCandidats == 0) {
        printf("Aucun candidat enregistre.\n");
        return;
    }

    /* Recherche du maximum */
    int maxVoix = 0;
    for (int i = 0; i < nbCandidats; i++)
        if (candidats[i].voix > maxVoix)
            maxVoix = candidats[i].voix;

    if (maxVoix == 0) {
        printf("Aucun vote exprime. Pas de gagnant.\n");
        return;
    }

    /* Compte les candidats a egalite */
    int nbGagnants = 0;
    for (int i = 0; i < nbCandidats; i++)
        if (candidats[i].voix == maxVoix)
            nbGagnants++;

    printf("========================================\n");
    if (nbGagnants == 1) {
        printf("   GAGNANT DU SCRUTIN\n");
        printf("========================================\n");
        for (int i = 0; i < nbCandidats; i++) {
            if (candidats[i].voix == maxVoix) {
                printf("   >> %s avec %d voix <<\n", candidats[i].nom, maxVoix);
            }
        }
    } else {
        printf("   EGALITE PARFAITE !\n");
        printf("========================================\n");
        printf("   Les candidats suivants sont a egalite avec %d voix :\n", maxVoix);
        for (int i = 0; i < nbCandidats; i++) {
            if (candidats[i].voix == maxVoix)
                printf("   >> %s <<\n", candidats[i].nom);
        }
    }
    printf("========================================\n");
}

/*
 * genererRapportFinal()
 * ----------------------
 * Cree le fichier rapport_final.txt avec :
 *   - Date et heure de generation
 *   - Nom de chaque candidat, voix, pourcentage
 *   - Votes blancs
 *   - Gagnant (ou egalite)
 *   - Taux de participation
 */
void genererRapportFinal(void)
{
    FILE *f = fopen(FICHIER_RAPPORT, "w");
    if (!f) {
        printf("[ERREUR] Impossible de creer le rapport final.\n");
        return;
    }

    /* Date et heure */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char dateBuf[64];
    strftime(dateBuf, sizeof(dateBuf), "%d/%m/%Y a %H:%M:%S", t);

    fprintf(f, "================================================\n");
    fprintf(f, "         RAPPORT FINAL - SCRUTIN PIVOTE\n");
    fprintf(f, "================================================\n");
    fprintf(f, "Genere le : %s\n\n", dateBuf);

    /* Statistiques de participation */
    int votants = 0, blancs = 0;
    for (int i = 0; i < nbElecteurs; i++) {
        if (electeurs[i].a_vote) {
            votants++;
            if (electeurs[i].vote_blanc) blancs++;
        }
    }
    double tauxParticipation = (nbElecteurs > 0)
                               ? (100.0 * votants / nbElecteurs)
                               : 0.0;

    fprintf(f, "------------------------------------------------\n");
    fprintf(f, "PARTICIPATION\n");
    fprintf(f, "------------------------------------------------\n");
    fprintf(f, "Electeurs inscrits : %d\n", nbElecteurs);
    fprintf(f, "Votes exprimes     : %d\n", votants);
    fprintf(f, "Votes blancs       : %d\n", blancs);
    fprintf(f, "Taux participation : %.1f%%\n\n", tauxParticipation);

    /* Resultats par candidat */
    int totalVoix = 0;
    for (int i = 0; i < nbCandidats; i++) totalVoix += candidats[i].voix;
    totalVoix += blancs;

    fprintf(f, "------------------------------------------------\n");
    fprintf(f, "RESULTATS PAR CANDIDAT\n");
    fprintf(f, "------------------------------------------------\n");
    for (int i = 0; i < nbCandidats; i++) {
        double pct = (totalVoix > 0) ? (100.0 * candidats[i].voix / totalVoix) : 0.0;
        fprintf(f, "  %-20s : %3d voix  (%.1f%%)\n",
                candidats[i].nom, candidats[i].voix, pct);
    }
    double pctBlanc = (totalVoix > 0) ? (100.0 * blancs / totalVoix) : 0.0;
    fprintf(f, "  %-20s : %3d voix  (%.1f%%)\n", "VOTE BLANC", blancs, pctBlanc);
    fprintf(f, "\n  Total votes : %d\n\n", totalVoix);

    /* Gagnant */
    fprintf(f, "------------------------------------------------\n");
    fprintf(f, "RESULTAT FINAL\n");
    fprintf(f, "------------------------------------------------\n");

    int maxVoix = 0;
    for (int i = 0; i < nbCandidats; i++)
        if (candidats[i].voix > maxVoix) maxVoix = candidats[i].voix;

    if (maxVoix == 0) {
        fprintf(f, "Aucun vote exprime. Pas de gagnant.\n");
    } else {
        int nbGagnants = 0;
        for (int i = 0; i < nbCandidats; i++)
            if (candidats[i].voix == maxVoix) nbGagnants++;

        if (nbGagnants == 1) {
            for (int i = 0; i < nbCandidats; i++) {
                if (candidats[i].voix == maxVoix)
                    fprintf(f, "GAGNANT : %s avec %d voix\n",
                            candidats[i].nom, maxVoix);
            }
        } else {
            fprintf(f, "EGALITE entre les candidats suivants (%d voix chacun) :\n", maxVoix);
            for (int i = 0; i < nbCandidats; i++)
                if (candidats[i].voix == maxVoix)
                    fprintf(f, "  - %s\n", candidats[i].nom);
        }
    }

    fprintf(f, "\n================================================\n");
    fprintf(f, "           FIN DU RAPPORT\n");
    fprintf(f, "================================================\n");

    fclose(f);
    printf("[INFO] Rapport final genere : %s\n", FICHIER_RAPPORT);
}

/* =========================================================
 * 5. PERSISTANCE DES DONNEES
 * ========================================================= */
void sauvegarderDonnees(void)
{
    FILE *f = fopen(FICHIER_SAUVEGARDE, "w");
    if (!f) return;
    fprintf(f, "%d\n%d\n", voteOuvert, nbElecteurs);
    for (int i = 0; i < nbElecteurs; i++)
        fprintf(f, "%d %s %d %d %s\n",
                electeurs[i].id, electeurs[i].nom,
                electeurs[i].a_vote, electeurs[i].vote_blanc,
                electeurs[i].username);
    fprintf(f, "%d\n", nbCandidats);
    for (int i = 0; i < nbCandidats; i++)
        fprintf(f, "%d %s %d\n",
                candidats[i].id, candidats[i].nom, candidats[i].voix);
    fclose(f);
}

void chargerDonnees(void)
{
    FILE *f = fopen(FICHIER_SAUVEGARDE, "r");
    if (!f) return;
    fscanf(f, "%d", &voteOuvert);
    fscanf(f, "%d", &nbElecteurs);
    for (int i = 0; i < nbElecteurs; i++)
        fscanf(f, "%d %s %d %d %s",
               &electeurs[i].id, electeurs[i].nom,
               &electeurs[i].a_vote, &electeurs[i].vote_blanc,
               electeurs[i].username);
    fscanf(f, "%d", &nbCandidats);
    for (int i = 0; i < nbCandidats; i++)
        fscanf(f, "%d %s %d",
               &candidats[i].id, candidats[i].nom, &candidats[i].voix);
    fclose(f);
    printf(">> Données chargées.\n");
}

void exporterVersExcel(void)
{
    FILE *f = fopen(FICHIER_EXCEL, "w");
    if (!f) return;
    fprintf(f, "ID Candidat;Nom Candidat;Nombre de Voix\n");
    for (int i = 0; i < nbCandidats; i++)
        fprintf(f, "%d;%s;%d\n",
                candidats[i].id, candidats[i].nom, candidats[i].voix);
    int blancs = 0;
    for (int i = 0; i < nbElecteurs; i++)
        if (electeurs[i].vote_blanc) blancs++;
    fprintf(f, "0;VOTE BLANC;%d\n", blancs);
    fclose(f);
}

/* =========================================================
 * 6. SERVEUR RESEAU (threads Windows)
 * ========================================================= */
DWORD WINAPI threadServeurReseau(LPVOID arg)
{
    WSADATA wsa;
    SOCKET  serveur, client;
    struct sockaddr_in addr;
    char   buffer[BUFFER];
    char   listeCandidatsStr[BUFFER];
    int    addrlen = sizeof(addr);

    WSAStartup(MAKEWORD(2,2), &wsa);
    serveur = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(serveur, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[ERREUR] Impossible de lier le port %d.\n", PORT);
        return 1;
    }
    listen(serveur, 5);
    printf(">> Serveur réseau ACTIF sur le port %d.\n", PORT);

    while (1) {
        client = accept(serveur, (struct sockaddr*)&addr, &addrlen);
        if (client == INVALID_SOCKET) continue;

        /* Etape 1 : Authentification */
        int recv_size = recv(client, buffer, BUFFER - 1, 0);
        if (recv_size <= 0) { closesocket(client); continue; }
        buffer[recv_size] = '\0';

        char cmd[16];
        char username[AUTH_MAX_USERNAME + 1];
        char password[AUTH_MAX_PASSWORD + 1];
        int  parsed = sscanf(buffer, "%15s %64s %64s", cmd, username, password);

        if (parsed != 3 || strcmp(cmd, "AUTH") != 0) {
            send(client, "AUTH_FAIL", 9, 0);
            closesocket(client);
            continue;
        }

        AuthUser uAuth;
        AuthStatus stAuth = auth_authenticate(CSV_PATH, username, password, &uAuth);
        if (stAuth != AUTH_OK || strcmp(uAuth.role, "votant") != 0) {
            send(client, "AUTH_FAIL", 9, 0);
            closesocket(client);
            continue;
        }
        send(client, "AUTH_OK", 7, 0);

        /* Etape 2 : Envoi liste candidats */
        strcpy(listeCandidatsStr, "\n--- LISTE DES CANDIDATS ---\n");
        char ligne[100];
        for (int k = 0; k < nbCandidats; k++) {
            sprintf(ligne, "[%d] %s\n", candidats[k].id, candidats[k].nom);
            strcat(listeCandidatsStr, ligne);
        }
        strcat(listeCandidatsStr, "[0] VOTE BLANC\n---------------------------\n");
        send(client, listeCandidatsStr, strlen(listeCandidatsStr), 0);

        /* Etape 3 : Reception du vote */
        recv_size = recv(client, buffer, BUFFER - 1, 0);
        if (recv_size <= 0) { closesocket(client); continue; }
        buffer[recv_size] = '\0';

        char cmd2[16];
        int  idE = -1, idC = -1;
        sscanf(buffer, "%15s %d %d", cmd2, &idE, &idC);

        int ok = 0;
        if (strcmp(cmd2, "VOTE") == 0 && voteOuvert) {
            for (int i = 0; i < nbElecteurs; i++) {
                if (electeurs[i].id == idE
                    && strcmp(electeurs[i].username, username) == 0
                    && electeurs[i].a_vote == 0)
                {
                    int candidatTrouve = 0;
                    for (int j = 0; j < nbCandidats; j++) {
                        if (candidats[j].id == idC) {
                            candidats[j].voix++;
                            candidatTrouve = 1;
                            break;
                        }
                    }
                    electeurs[i].vote_blanc = candidatTrouve ? 0 : 1;
                    electeurs[i].a_vote = 1;
                    ok = 1;
                    break;
                }
            }
        }

        if (ok) {
            send(client, "OK", 2, 0);
            sauvegarderDonnees();
            exporterVersExcel();
        } else {
            send(client, "ERREUR", 6, 0);
        }
        closesocket(client);
    }
    return 0;
}

DWORD WINAPI threadAffichageTempsReel(LPVOID arg)
{
    while (affichageAutoActif) {
        system("cls");
        printf("===== CONTROLE EN TEMPS REEL =====\n");
        afficherBarresASCII();
        printf("\n");
        afficherStatistiques();
        printf("\n[INFO]:Fichier Excel mis à jour automatiquement.\n");
        printf("Appuie sur une touche du menu pour quitter...\n");
        Sleep(3000);
    }
    return 0;
}

void lancerServeurReseau(void)
{
    HANDLE thread = CreateThread(NULL, 0, threadServeurReseau, NULL, 0, NULL);
<<<<<<< HEAD
    if (!thread)
    {
        printf("Erreur thread réseau.\n");
        return;
    }
=======
    if (!thread) { printf("Erreur thread reseau.\n"); return; }
>>>>>>> 22517f3359c7fb66249c4c6430fbfca0a6d55766
    affichageAutoActif = 1;
    CreateThread(NULL, 0, threadAffichageTempsReel, NULL, 0, NULL);
    printf("Mode réseau actif. Appuyez sur 0 pour quitter proprement.\n");
}

/* =========================================================
 * 7. GESTION DES COMPTES UTILISATEURS
 * ========================================================= */
void menu_inscription_admin(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char password[AUTH_MAX_PASSWORD + 1];
    char role[AUTH_MAX_ROLE + 1];

    lire_ligne_srv("Identifiant : ", username, sizeof(username));
    lire_ligne_srv("Mot de passe : ", password, sizeof(password));
    lire_ligne_srv("Role (votant/admin) : ", role, sizeof(role));

    AuthStatus st = auth_register_user(CSV_PATH, username, password, role);
    if (st == AUTH_OK)
        printf("Utilisateur créé avec succes.\n");
    else if (st == AUTH_ERR_EXISTS)
        printf("Erreur : identifiant déjà existant.\n");
    else
        printf("Erreur creation (code=%d).\n", st);
}

void menu_changer_mdp(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char old_password[AUTH_MAX_PASSWORD + 1];
    char new_password[AUTH_MAX_PASSWORD + 1];

    lire_ligne_srv("Identifiant : ", username, sizeof(username));
    lire_ligne_srv("Ancien mot de passe : ", old_password, sizeof(old_password));
    lire_ligne_srv("Nouveau mot de passe : ", new_password, sizeof(new_password));

    AuthStatus st = auth_change_password(CSV_PATH, username, old_password, new_password);
    if (st == AUTH_OK)
        printf("Mot de passe mis a jour.\n");
    else if (st == AUTH_ERR_NOTFOUND)
        printf("Utilisateur inconnu.\n");
    else if (st == AUTH_ERR_INVALID)
        printf("Ancien mot de passe incorrect.\n");
    else
        printf("Erreur (code=%d).\n", st);
}

void menu_reinitialiser_mdp(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char new_password[AUTH_MAX_PASSWORD + 1];

    printf("\n[REINITIALISATION MOT DE PASSE]\n");
    printf("Reservée a l'administrateur (sans controle de l'ancien mdp).\n\n");

    lire_ligne_srv("Identifiant de l'électeur : ", username, sizeof(username));
    lire_ligne_srv("Nouveau mot de passe       : ", new_password, sizeof(new_password));

    AuthStatus st = auth_change_password(CSV_PATH, username, NULL, new_password);
    if (st == AUTH_OK)
        printf("Mot de passe de '%s' reinitialisé.\n", username);
    else if (st == AUTH_ERR_NOTFOUND)
        printf("Utilisateur inconnu.\n");
    else
        printf("Erreur (code=%d).\n", st);
}

void menu_activation(int activer)
{
    char username[AUTH_MAX_USERNAME + 1];
    lire_ligne_srv("Identifiant : ", username, sizeof(username));

    AuthStatus st = auth_set_active(CSV_PATH, username, activer);
    if (st == AUTH_OK)
        printf("Compte %s %s.\n", username, activer ? "activé" : "désactivé");
    else if (st == AUTH_ERR_NOTFOUND)
        printf("Utilisateur inconnu.\n");
    else
        printf("Erreur (code=%d).\n", st);
}

void menu_lister(void)
{
    AuthUser *users = NULL;
    size_t    count = 0;

    AuthStatus st = auth_list_users(CSV_PATH, &users, &count);
    if (st != AUTH_OK) {
        printf("Impossible de lire la liste (code=%d).\n", st);
        return;
    }
    printf("Utilisateurs (%zu) :\n", count);
    for (size_t i = 0; i < count; ++i)
        afficher_utilisateur(&users[i]);
    auth_free_user_list(users);
}

void afficherMenuGestionNavigue(int sel)
{
<<<<<<< HEAD
    const char *options[] = {
        "1. Créer un compte (admin/autre)",
        "2. Changer un mot de passe",
        "3. Activer un compte",
        "4. Désactiver un compte",
        "5. Lister les utilisateurs",
        "6. Réinitialiser le mot de passe d'un électeur",
        "0. Retour"
    };
    int nbOptions = 7;

    system("cls");

    setCouleur(12); /* Rouge — même style que le menu principal */
    printf("\n  ===== GESTION DES COMPTES =====\n\n");

    for (int i = 0; i < nbOptions; i++) {
        if (i == sel) {
            setCouleur(COULEUR_SELEC);
            printf("  > %-50s <\n", options[i]);
        } else {
            setCouleur(COULEUR_NORMAL);
            printf("    %-50s\n", options[i]);
        }
    }

    setCouleur(COULEUR_NORMAL);
    printf("\n  [Flèches pour naviguer  |  Entrée pour valider]\n");
=======
    int  choix;
    char buf[32];
    do {
        printf("\n=== Gestion des comptes ===\n");
        printf("1. Creer un compte (admin/autre)\n");
        printf("2. Changer un mot de passe\n");
        printf("3. Activer un compte\n");
        printf("4. Desactiver un compte\n");
        printf("5. Lister les utilisateurs\n");
        printf("6. Reinitialiser le mot de passe d'un electeur\n");
        printf("0. Retour\n");
        lire_ligne_srv("Choix : ", buf, sizeof(buf));
        if (sscanf(buf, "%d", &choix) != 1) choix = -1;
        switch (choix) {
            case 1: menu_inscription_admin(); break;
            case 2: menu_changer_mdp();       break;
            case 3: menu_activation(1);       break;
            case 4: menu_activation(0);       break;
            case 5: menu_lister();            break;
            case 6: menu_reinitialiser_mdp(); break;
            case 0: break;
            default: printf("Choix invalide.\n"); break;
        }
    } while (choix != 0);
>>>>>>> 22517f3359c7fb66249c4c6430fbfca0a6d55766
}

void menuGestionComptes(void)
{
    /* Table index -> numéro d'option réel */
    static const int indexVersOptionGestion[] = {
        1, 2, 3, 4, 5, 6, 0
    };

    int sel     = 0;
    int nbItems = 7;
    int choix   = -1;
    int touche;

    do {
        afficherMenuGestionNavigue(sel);

        /* Navigation flèches */
        while (1) {
            touche = _getch();
            if (touche == 0 || touche == 224) {
                touche = _getch();
                if (touche == 72) {         /* Flèche HAUT */
                    sel = (sel - 1 + nbItems) % nbItems;
                    afficherMenuGestionNavigue(sel);
                } else if (touche == 80) {  /* Flèche BAS */
                    sel = (sel + 1) % nbItems;
                    afficherMenuGestionNavigue(sel);
                }
            } else if (touche == 13) {      /* Entrée */
                choix = indexVersOptionGestion[sel];
                setCouleur(COULEUR_NORMAL);
                system("cls");
                break;
            }
        }

        switch (choix) {
            case 1: menu_inscription_admin(); break;
            case 2: menu_changer_mdp();       break;
            case 3: menu_activation(1);       break;
            case 4: menu_activation(0);       break;
            case 5: menu_lister();            break;
            case 6: menu_reinitialiser_mdp(); break;
            case 0: break;
            default: break;
        }

        /* Pause après chaque action sauf Retour */
        if (choix != 0) {
            printf("\n  Appuyez sur une touche pour revenir...");
            _getch();
        }

    } while (choix != 0);
}

/* =========================================================
 * AFFICHAGE DU MENU AVEC OPTION SURLIGNÉE
 * ========================================================= */
void afficherMenuNavigue(int sel)
{
    #define COULEUR_TITRE   12   /* Rouge clair */

    /* Labels des options — avec accents grâce au code page 1252 */
    const char *options[] = {
        "1.  Ajouter un électeur",
        "2.  Afficher les électeurs",
        "3.  Ajouter un candidat",
        "4.  Afficher les candidats",
        "5.  Ouvrir le vote",
        "6.  Fermer le vote  [=> gagnant + rapport auto]",
        "7.  Les résultats (texte)",
        "8.  Les résultats (barres ASCII)",
        "9.  Les statistiques",
        "10. Lancer le mode RÉSEAU",
        "11. Exporter vers Excel",
        "12. Gestion des comptes",
        "0.  Quitter ET RÉINITIALISER"
    };
    int nbOptions = 13;

    system("cls");

    setCouleur(COULEUR_TITRE);
    printf("\n  ===== MENU PIVOTE ADMINISTRATEUR =====\n\n");

    for (int i = 0; i < nbOptions; i++) {
        if (i == sel) {
            /* Option sélectionnée : cyan clair + indicateur > */
            setCouleur(COULEUR_SELEC);
            printf("  > %-48s <\n", options[i]);
        } else {
            setCouleur(COULEUR_NORMAL);
            printf("    %-48s\n", options[i]);
        }
    }

    setCouleur(COULEUR_NORMAL);
    printf("\n  [Flèches pour naviguer  |  Entrée pour valider]\n");
}

/* =========================================================
 * NAVIGATION AU CLAVIER (flèches + Entrée)
 * ========================================================= */

/* Table de correspondance : index dans le menu -> numéro d'option réel */
static const int indexVersOption[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 0
};

int naviguerMenu(void)
{
    int sel     = 0;   /* index courant dans la liste */
    int nbItems = 13;  /* nombre total d'options */
    int touche;

    afficherMenuNavigue(sel);

    while (1) {
        touche = _getch();

        /* Les flèches envoient 2 codes : 0 ou 224, puis le code réel */
        if (touche == 0 || touche == 224) {
            touche = _getch();

            if (touche == 72) {          /* Flèche HAUT */
                sel = (sel - 1 + nbItems) % nbItems;
                afficherMenuNavigue(sel);
            }
            else if (touche == 80) {     /* Flèche BAS */
                sel = (sel + 1) % nbItems;
                afficherMenuNavigue(sel);
            }
        }
        else if (touche == 13) {         /* Touche ENTRÉE */
            setCouleur(COULEUR_NORMAL);
            system("cls");
            return indexVersOption[sel];
        }
    }
}




/* =========================================================
 * 8. MENU PRINCIPAL ADMINISTRATEUR
 * ========================================================= */
void menuServeur(void)
{
    int choix;
    do {
<<<<<<< HEAD
        /* Navigation avec flèches au lieu de scanf */
        choix = naviguerMenu();
=======
        printf("\n===== MENU PIVOTE ADMINISTRATEUR =====\n");
        printf("1.  Ajouter un electeur\n");
        printf("2.  Afficher les electeurs\n");
        printf("3.  Ajouter un candidat\n");
        printf("4.  Afficher les candidats\n");
        printf("5.  Ouvrir le vote\n");
        printf("6.  Fermer le vote  [=> gagnant + rapport auto]\n");
        printf("7.  Les resultats (texte)\n");
        printf("8.  Les resultats (barres ASCII)\n");
        printf("9.  Les statistiques\n");
        printf("10. Lancer le mode RESEAU\n");
        printf("11. Exporter vers Excel\n");
        printf("12. Gestion des comptes\n");
        printf("0.  Quitter ET REINITIALISER\n");
        printf("Choix : ");
        scanf("%d", &choix);
        vider_buffer_stdin();
>>>>>>> 22517f3359c7fb66249c4c6430fbfca0a6d55766

        switch (choix) {
        case 1:  ajouterElecteur();      sauvegarderDonnees(); break;
        case 2:  afficherElecteurs();    break;
        case 3:  ajouterCandidat();      sauvegarderDonnees(); break;
        case 4:  afficherCandidats();    break;
        case 5:  ouvrirVote();           sauvegarderDonnees(); break;
        case 6:  fermerVote();           sauvegarderDonnees(); break;
        case 7:  afficherResultats();    break;
        case 8:  afficherBarresASCII();  break;
        case 9:  afficherStatistiques(); break;
        case 10: lancerServeurReseau();  break;
        case 11:
            exporterVersExcel();
<<<<<<< HEAD
            printf("Fichier Excel généré !\n");
=======
            printf("Fichier Excel genere !\n");
>>>>>>> 22517f3359c7fb66249c4c6430fbfca0a6d55766
            break;
        case 12:
            menuGestionComptes();
            break;
        case 0:
            affichageAutoActif = 0;
            remove(FICHIER_SAUVEGARDE);
            printf(">> Session terminée. Fichiers de sauvegarde supprimés.\n");
            break;
        default:
            printf("Choix invalide.\n");
            break;
        }
<<<<<<< HEAD

        /* Pause après chaque action pour lire le résultat */
        if (choix != 0 && choix != 10) {
            printf("\n  Appuyez sur une touche pour revenir au menu...");
            _getch();
        }

=======
>>>>>>> 22517f3359c7fb66249c4c6430fbfca0a6d55766
    } while (choix != 0);
}

/* =========================================================
 * 9. CONNEXION ADMINISTRATEUR
 * ========================================================= */
int ecranConnexionAdmin(void)
{
    char username[AUTH_MAX_USERNAME + 1];
    char password[AUTH_MAX_PASSWORD + 1];

    printf("\n===================================================\n");
    printf("          PIVOTE - ESPACE ADMINISTRATEUR\n");
    printf("===================================================\n");

    AuthUser *users = NULL;
    size_t    count = 0;
    int adminExiste = 0;
    if (auth_list_users(CSV_PATH, &users, &count) == AUTH_OK) {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(users[i].role, "admin") == 0) {
                adminExiste = 1;
                break;
            }
        }
        auth_free_user_list(users);
    }

    if (!adminExiste) {
        printf("\n[PREMIERE UTILISATION] Aucun administrateur trouve.\n");
        printf("Veuillez creer le compte administrateur principal :\n");
        lire_ligne_srv("Identifiant admin  : ", username, sizeof(username));
        lire_ligne_srv("Mot de passe admin : ", password, sizeof(password));
        AuthStatus st = auth_register_user(CSV_PATH, username, password, "admin");
        if (st != AUTH_OK) {
            printf("Erreur creation admin (code=%d).\n", st);
            return 0;
        }
        printf("Compte admin cree. Veuillez vous connecter.\n\n");
    }

    int tentatives = 3;
    while (tentatives > 0) {
        lire_ligne_srv("Identifiant : ", username, sizeof(username));
        lire_ligne_srv("Mot de passe : ", password, sizeof(password));

        AuthStatus st = auth_authenticate(CSV_PATH, username, password, &adminConnecte);
        if (st == AUTH_OK && strcmp(adminConnecte.role, "admin") == 0) {
            printf("\nAuthentification reussie. Bonjour %s !\n", adminConnecte.username);
            return 1;
        }
        tentatives--;
        if (tentatives > 0)
            printf("Identifiants incorrects ou compte non-admin. %d tentative(s) restante(s).\n", tentatives);
        else
            printf("Trop de tentatives. Acces refuse.\n");
    }
    return 0;
}
