# Revue de code — correctifs appliqués

Base : awalol/DS5Dongle @ 77d378e. Original intact dans `../DS5Dongle.orig` pour diff.
**Non compilé ni testé sur matériel.**

| Fichier | Problème | Correctif |
|---|---|---|
| src/main.cpp (`tud_hid_set_report_cb`, cmds 0xF6–0xFB) | `memcpy` de `bufsize` octets dans un buffer fixe sans borne (l'autre branche bornait déjà) | Taille bornée à `CFG_TUD_HID_EP_BUFSIZE` |
| src/bt.cpp (`HCI_EVENT_LINK_KEY_REQUEST`) | La clé d'appairage BT (secret) était imprimée sur le port série à chaque reconnexion, et une clé non initialisée quand il n'y en avait pas | Affichage seulement en build `ENABLE_VERBOSE` et seulement si la clé existe |
| src/bt.cpp (inquiry complete) | `GAP_EVENT_INQUIRY_COMPLETE` et `HCI_EVENT_INQUIRY_COMPLETE` peuvent arriver tous les deux → `create_connection` en double → l'échec du 2e remet `new_pair=false` en plein appairage | Garde `!new_pair` |
| src/bt.cpp (canal contrôle L2CAP) | Lecture de `packet[0]`, `packet[23]` et `assign(packet+2, packet+size)` sans vérifier `size` (lecture hors limites / plage inversée = UB) | Vérifications de taille |
| src/bt.cpp (échec d'authentification) | Relance l'inquiry sans `bt_inquiring = true` → LED ne clignote pas | Flag mis à jour |
| src/bt.cpp (déconnexion) | Callback appelé sans test de nullité | Test ajouté |
| src/bt.cpp (`bt_write`) | Pas de garde contre un paquet > 672 octets | Garde ajoutée |
| src/config.cpp (`set_config`) | Sauver la config depuis l'outil allumait la LED fixe même sans manette (et cassait le clignotement d'appairage) | LED = connecté && !disable_pico_led |

## Améliorations (2e passe)

| Zone | Changement |
|---|---|
| Latence (main.cpp, bt.cpp) | Les erreurs `tud_hid_report` et "FIFO plein" sont limitées à 1 log/s (avant : un `printf` par itération de boucle quand l'hôte ne lit plus, ce qui crée de la gigue). |
| `interrupt_loop` (main.cpp) | Factorisé (le TODO d'origine) : un seul chemin d'envoi pour les modes à fréquence fixe et temps réel, comportement identique. |
| Config (config.cpp) | `config_load()` vérifie le CRC. Un secteur corrompu (coupure pendant l'écriture) repasse aux valeurs par défaut au lieu de charger des remaps/raccourcis invalides. Les configs sauvegardées normalement ne sont pas touchées, car `config_save()` a toujours écrit le CRC. |
| Appairage (bt.cpp) | Recherche relancée automatiquement après 30 s sans connexion : une manette en mode PS+Create est trouvée sans appuyer sur BOOTSEL. La reconnexion par PS seul n'est pas affectée. Désactivable avec `-DBT_AUTO_INQUIRY_INTERVAL_MS=0`. |
| Nettoyage | Suppression des `main()` commentés (bt.cpp, usb.cpp), des variables `channelNum` inutilisées et d'un format `%d` appliqué à un `float`. |

## À tester en priorité sur le matériel
1. Appairage d'une nouvelle manette (PS+Create), avec et sans clic BOOTSEL.
2. Reconnexion par PS seul pendant une recherche automatique en cours.
3. Démarrage avec une config existante : elle doit être conservée.
4. Les 4 modes de polling_rate.
