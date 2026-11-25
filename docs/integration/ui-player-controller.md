# Checklist d'intégration : ui-player-controller (6a) + drop-despawn (6b)
Branche `feature/ui-player-controller`. État lu via MCP sur les **anciens binaires** (C++ non compilé).

**Verdict : 0 étape [MCP] sûre, ne pas lancer `bp-integrator`.** Tout est graphe, interface de classe ou tableau de structs (`set_default` d'un `TArray<FUIWidgetEntry>` à références d'assets : non vérifié, donc [HUMAIN]). Le C++ porte la logique, l'humain câble plus vite seul.

## Constats éditeur
- `BP_FirstPersonCharacter` (EventGraph, 72 nœuds) : 6 `Create Widget` (ResourceCounter, StaminaBar, CauldronProgress, ControlsPanel, ServantTaskMenu x2) + 6 `Add to Viewport`, variables `ControlsPanelWidget` / `ServantTaskMenu`, évènements `OnToggleControlsPanel` / `OnServantMenuToggled`, défauts `ToggleControlsAction`=`IA_ToggleControls`, `ToggleServantMenuAction`=`IA_ToggleServantMenu`. Aucun `Delay`, une seule `Branch` : les 3 HUD naissent à `Event BeginPlay` (déduit des titres de nœuds, **liens non lus**). Aucun ZOrder explicite (tous 0).
- Chaudron : `GetActorOfClass(BP_Cauldron)` puis `InitializeCauldronProgress`. Mise à jour par `Tick` du WBP (`GetBrewProgress` > `SetPercent`, `Is Valid`). 1 seul `BP_Cauldron` dans `Lvl_Principal`.
- **Debug à conserver** dans ce graphe (TODO, avant Release) : `DebugKill`, `StartWaves`, 2 `Add Resource`, 2 `GetAllActorsOfClass`. Ne pas les supprimer ni les déconnecter du `BeginPlay`.
- `WBP_StaminaBar` / `WBP_CauldronProgress` : variable + `Tick` + `Is Valid` (composant nul toléré). `WBP_ResourceCounter` > 6 `WBP_ResourceEntry`, dont `InitializeEntry` fait `Bind Event to On Resource Changed` **sans Unbind** : pas idempotent (écart, étape 3). Racines de tous les WBP : `SelfHitTestInvisible`.
- `BP_FirstPersonPlayerController` : parent `ASpooky_SporesPlayerController`, `DefaultMappingContexts`=`IMC_Default` (appliqué par le contrôleur), pas de variable. `BP_FirstPersonGameMode` (aussi override du monde) : `PlayerControllerClass` = ce BP, pawn = `BP_FirstPersonCharacter`. `IMC_Default` : Entrée > `IA_ToggleControls`, Tab > `IA_ToggleServantMenu` (`bTriggerWhenPaused`=True, lu ; l'autre non pausante, voulu).
- Références aux membres retirés : seul `BP_FirstPersonCharacter` (recherche texte /Game + acteurs du niveau). Level Blueprint non lisible.
- Drops : `BP_Drop_Seed/Stone/Wheat/Wood`, parent `AResourceDrop`, sans composant ni variable ni fonction d'interface. Preset de collision, liste `Interfaces` et `DropLifetime` non lisibles (propriété absente des anciens binaires).

## Pré-requis
- Branche `feature/ui-player-controller`, éditeur fermé puis rouvert après rebuild complet (voir « Compilation »). Pas de PIE en cours.
- Classes à retrouver dans l'éditeur : `UIEntries` / `OpenUI` sur le contrôleur, interface `Player UI Widget`, `DropLifetime` sur les drops.

## Étapes (ordre obligatoire : contrôleur, WBP, Character, vérifs)
1. [HUMAIN] `/Game/Spooky_Spores/Core/Controller/BP_FirstPersonPlayerController` > Class Defaults > UI > `UIEntries` : 5 entrées (Id, classe, ZOrder, bOpenAtStart, bPausesGame, ToggleAction). ResourceCounter `WBP_ResourceCounter` Z0 open ; StaminaBar `WBP_StaminaBar` Z1 open ; CauldronProgress `WBP_CauldronProgress` Z2 open ; ControlsPanel `WBP_ControlsPanel` Z10, `IA_ToggleControls`, ni open ni pause ; ServantMenu `WBP_ServantTaskMenu` Z20, `IA_ToggleServantMenu`, pause. Règle : HUD < menus. Vérif : 5 éléments, Ids uniques, compile sans erreur.
2. [HUMAIN] `.../UI/Widgets/WBP_StaminaBar` : Class Settings > Interfaces > Add `Player UI Widget` ; Event `Initialize For Player` > `Is Valid(Pawn)` : oui > `Get Component by Class(StaminaComponent)` sur Pawn > `InitializeStaminaBar` ; non > `InitializeStaminaBar(None)`. Vérif : compile OK, pas de « Accessed None » pour un Pawn nul.
3. [HUMAIN] `.../WBP_ResourceEntry` > `InitializeEntry` : en tête, `Is Valid(Counter)` > `Unbind Event from On Resource Changed` (Target = ancien Counter, Event = `OnCounterChanged`), puis Set + Bind seulement si le nouveau Counter est valide. Vérif : un ramassage = +1 sur la ligne (pas +2) après plusieurs `InitializeCounter`.
4. [HUMAIN] `.../WBP_ResourceCounter` : interface `Player UI Widget` ; `Initialize For Player` > même schéma avec `ResourceCounterComponent` > `InitializeCounter` (None si Pawn nul). Vérif : compile OK.
5. [HUMAIN] `.../WBP_CauldronProgress` : interface ; `Initialize For Player` > `Get Actor Of Class(BP_Cauldron)` > `InitializeCauldronProgress` (sans le Pawn ; exception assumée, un seul chaudron). Vérif : compile OK.
6. [HUMAIN] `WBP_ControlsPanel` : rien. `WBP_ServantTaskMenu` : rien, l'interface est héritée du C++ (ne pas l'ajouter ; `Class Settings` la montre déjà). Vérif : compile OK.
7. [HUMAIN] `.../Core/Character/BP_FirstPersonCharacter` : compiler. **Erreurs attendues** sur `OnToggleControlsPanel` / `OnServantMenuToggled` (fonctions C++ retirées) : transitoires.
8. [HUMAIN] Supprimer ces 2 évènements et leurs corps (`Is Valid`/`Is Visible`, `Branch`, 5 `Set Visibility`, `Get Servant Subsystem` x2, `InitializeMenu` x2).
9. [HUMAIN] Supprimer les 6 `Create Widget`, 6 `Add to Viewport`, leurs `Get Player Controller`, `InitializeCounter`, `InitializeStaminaBar`, `InitializeCauldronProgress`, `GetActorOfClass(BP_Cauldron)`, 2 `GetComponentByClass`, `Set/Get ControlsPanelWidget` et `ServantTaskMenu`. Rebrancher `Event BeginPlay` vers les nœuds debug `Add Resource`.
10. [HUMAIN] Supprimer les variables `ControlsPanelWidget` et `ServantTaskMenu` (0 référence restante), compiler, sauvegarder. Vérif (relisible) : 0 erreur ; `Create Widget` absent ; `DebugKill`, `StartWaves`, 2 `Add Resource` présents ; `ToggleControlsAction` / `ToggleServantMenuAction` introuvables.
11. [HUMAIN] Ouvrir les 4 `BP_Drop_*` (`Placeables/Resources/`) : `Interfaces` vide (pas `Grabbable`), `DropLifetime`=300 non surchargé, `MeshComponent` en preset `ResourceDrop` (pas `PhysicsInteractable`). Tout écart : le signaler, ne pas deviner.
12. [HUMAIN] Tout sauvegarder. `git status` : `Lvl_Principal.umap` absente (non touchée).

## Compilation et rechargement
Fermer l'éditeur, régénérer les fichiers de projet (clic droit `.uproject`), **Rebuild complet** dans Rider (interface + `UFUNCTION` modifiées : pas de Live Coding), lancer les Automation Specs (`SpookySpores`), rouvrir l'éditeur sur la branche.

## Test PIE par Timéo
1. Démarrage : compteur de ressources, barre de stamina et progression du chaudron affichés. `ControlsPanel` masqué : l'était-il avant ? Sinon `bOpenAtStart` = vrai (l'ancien graphe ne semblait pas le masquer, non confirmé).
2. Entrée ouvre/ferme le panneau, sans pause, déplacement possible.
3. Tab : menu ouvert, monde figé (stamina, drops), curseur visible ; Tab ferme (le point de risque : focus Slate sur un bouton) ; curseur masqué, vue reprise.
4. Menu ouvert, HUD derrière : sélectionner un serviteur puis une catégorie marche (clics non avalés), liste et surbrillance se rafraîchissent.
5. Fermé : clic gauche (sort), clic droit (lancer), E, Maj, L marchent (un HUD `Visible` plein écran ne doit pas avaler les clics).
6. HUD vivants : stamina baisse au sprint et remonte ; ramassage = +1 par ligne ; progression du chaudron évolue pendant une préparation. Aucun moyen de changer de pawn aujourd'hui : un HUD correct au démarrage (le contrôleur naît avant le pawn) valide le chemin `OnPossess`.
7. Output Log : aucun `Error` `UIEntries`, aucun « Accessed None ».
8. Drops : sur `BP_Drop_Wood`, mettre `DropLifetime`=10, récolter du bois, le drop disparaît sans crédit à 10 s ; pause Tab avant : le délai ne s'écoule pas ; ramassage avant : crédit unique. Puis **flèche Reset to Default** (pas retaper 300 : l'override figé ignorerait un futur changement C++, CLAUDE.md section 8).
