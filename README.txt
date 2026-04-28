Bomberman

Grupas dalībnieki:
- Mārcis Deguts
- Gustavs Krasņikovs

Ieguldījums:
- Mārcis: 50%
- Gustavs: 50%

Īss apraksts:
Daudzspēlētāju Bomberman projekts C valodā ar autoritatīvu klienta-servera arhitektūru. Serveris uztur spēles stāvokli, bet klienti sūta ievadi un attēlo spēli. Projektu var palaist ar diviem dažādiem klientiem - tekstuālais(client) ar `ncurses` vai grafiskais(client-v2) ar `raylib`.

Palaišanas instrukcija:
1. Uzinstalēt bibliotēkas:
   ./install_dependencies.sh
2. Palaist serveri lokāli, ne obligāti vajag, jo serveris arī darbojas attālināti:
   ./run_bomberman.sh server
3. Atsevišķos termināļos palaist klientus:
   ./run_bomberman.sh client Alice
   ./run_bomberman.sh client Bob

Papildu `raylib` klients:
   ./run_bomberman.sh client-v2 Alice

Piezīme:
- pēc noklusējuma klienti pieslēdzas attālinātajam serverim
- ar parametru `-l` klients pieslēdzas lokālajam serverim, piemēram:
  ./run_bomberman.sh client Alice -l
  ./run_bomberman.sh client-v2 Alice -l
