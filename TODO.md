# TODO — Spooky_Spores

Seul le reste à faire figure ici. Le déjà fait est dans `git log` et `documentation-technique.md`. Pas de découpage par semaines : on déroule la file (règles : `CLAUDE.md` section 9). Release GitHub à chaque étape significative (`v0.2`, `v0.3`...).

## Prochaine session
- **État** : 3b (ennemis : déplacement NavMesh en `Engage` + réservation souple des cibles) terminée, validée en PIE et fusionnée. 2.2c (rôle « attaque/défense » des serviteurs) terminée et validée en PIE par Timéo le 2026-11-24. Item 6 (UI possédée par le PlayerController + despawn des drops) terminé et validé en PIE par Timéo le 2026-11-25. Item 4a (menu de paramètres, FOV seul, extensible) terminé et validé en PIE par Timéo le 2026-11-26.
- **Prochaine tâche** : item 7 (vrai inventaire), tête de la file. File réordonnée le 2026-11-25 selon les dépendances (6 avant tout nouveau menu, 7 avant 8 et 15, 9 avant la roue de sorts, sauvegarde après stabilisation de l'état).
- **Rappels** : la dette « Avant toute Release » (section Dette technique) ; points chauds à ne pas modifier en parallèle : `EResourceType`, `*.Build.cs`, `Default*.ini`, `TODO.md`, `CLAUDE.md`, `documentation-technique.md`, `Lvl_Principal.umap` (`CLAUDE.md` section 10) ; plugins d'outillage (EngineWire, pont MCP) : compilés sur une seule machine, **l'autre reste à faire** (procédure : `CLAUDE.md` section 7).

## En cours
(rien)

## File d'attente (dans l'ordre)
Les numéros sont des identifiants stables (cités ailleurs : « item 6 », « items 7-8 »...) : les items faits sont retirés sans renuméroter. **L'ordre de lecture fait foi, pas l'ordre des numéros** (file réordonnée le 2026-11-25 ; les suffixes `a`/`b` scindent un item sans le renuméroter).
7. Vrai inventaire (stacks, slots, UI liste)
   - Cible de Timéo : « un objet dans la base à côté du coeur du joueur qui va en fait être un inventaire virtuel à distance qui sera en fait mon inventaire et celui des serviteurs, une sorte d'inventaire partagé en remote dans lequel les serviteurs pourront piocher pour replanter ». Décision de récolte : « quand c'est le serviteur qui récolte, les drops n'apparaissent pas mais au contraire que ça s'ajoute directement dans l'inventaire partagé ».
   - Stockage provisoire (décidé le 2026-11-09) : c'est le compteur du joueur 0 (`UResourceCounterComponent`), la Necrospore y est une ressource comptée. Chaque accès reste isolé à un endroit précis, pour que le passage au vrai inventaire ne touche que ceux-là : pose de Necrospore, `DoPlaceServant` (`TrySpendResource`) ; récolte serviteur, dans `AServantAIController` (même fichier), `FindHarvestStorage` (résolution), `HarvestAndDeposit` (crédit), `TryPlantOnField` (dépense des graines) et `WatchStorage` (abonnement à `OnResourceChanged`).
5a. Conception de la sauvegarde : spec seule, sans implémentation (voir « Sauvegarde » plus bas). Après l'inventaire, pour que chaque spec suivante précise « ce que ce système sauvegarde »
8. UI drag-and-drop + barre de raccourcis (9 emplacements). Le menu des serviteurs passe de la sélection par clic au glisser-déposer (seul le widget change, l'API C++ reste)
9. Objets équipables + contrôles contextuels. Refonte des entrées (Input Mapping Contexts) ; tranche le conflit du clic droit (lancer / roue de sorts) ; refaire la touche provisoire **O** du menu des paramètres, et ajouter éventuellement un bouton Fermer (aujourd'hui seule O ferme le menu)
4b. Remappage des touches (Enhanced Input, Player Mappable Keys), **après l'item 9** : remapper avant la refonte des contextes serait du travail jeté. S'ajoute comme un réglage du menu des paramètres (`documentation-technique.md` section 16)
10. Roue de sorts (clic droit maintenu) + mana généralisé (4 emplacements, recharge ~5 s)
11. Sorts 2 et 3
12. Sort 4
18. Passe d'équilibrage : placeholders du spawner de vagues et du combat (voir Dette technique, « À régler »), une fois les dégâts du joueur en place
13. Vue de gestion top-down (pause à l'entrée) — **indissociable de l'item 14**. Réutiliser `bPausesGame` / `ApplyModalState` du PlayerController (item 6)
14. Construction façon city-builder (grille, placement, rotation, déplacement, aperçu vert/rouge). Cases du chemin vers le village verrouillées en permanence (`GDD.md` section 3). Indissociable du 13. L'agrandissement de la base doit mettre à jour `BP_BaseZone`.
15. Construction ↔ ressources (coût, effets des bâtiments) ; traiter l'affichage de progression de plusieurs chaudrons (`WBP_CauldronProgress`, voir Dette technique)
5b. Implémentation de la sauvegarde, une fois l'état stable (inventaire, sorts, bâtiments), en un seul passage
16. Zone de fin : trigger box le long du chemin, accessible après un seuil de vagues survécues (à fixer par playtest) **et** accès garanti quand `AEnemyWaveSpawner::OnAllWavesCleared` est émis (20 vagues tuées par défaut, village vide : le joueur a gagné par défaut, même sans la puissance nécessaire, `GDD.md` section 4), indicateur visuel (`GDD.md` section 2)
17. Victoire + clôture narrative : à l'entrée de la zone de fin, choix terminer / continuer ; cinématique à variantes selon le nombre de serviteurs (`GDD.md` section 2)

## Sauvegarde (conception : item 5a, implémentation : item 5b)
Positions des objets saisissables · nœuds récoltés et drops au sol · `bHasTriggered` des `AStoryTrigger` · ressources du `UResourceCounterComponent` · plus tard : plantations, serviteurs, potions, état de la base. Données externes : c'est ici qu'on durcit (NaN, valeurs hors bornes).

## Dette technique

**Avant toute Release**
- [ ] **Avant toute Release** : supprimer `AEnemySoldier::DebugKill` et sa touche dans `BP_FirstPersonCharacter`, ainsi que la touche de test `StartWaves` (même Blueprint) ; supprimer le nœud de test `Add Resource (Necrospore)` du `BeginPlay` de `BP_FirstPersonCharacter` ; remettre `Hidden In Game` à `true` sur `ZoneBox` de `BP_BaseZone`

**À régler (valeurs et réglages)**
- [ ] Placeholders du spawner de vagues à régler (instance dans `Lvl_Principal`) : `Count` des 20 entrées de `Waves` (3 chacune aujourd'hui) ; `FirstWaveDelay` 180 s non validé ; `WaveInterval` 180 s à réécrire au game feeling. **`WaveInterval` vaut 10 s sur l'instance du niveau (valeur de test, le défaut C++ est 180)** : à rétablir avant de juger le rythme
- [ ] Placeholders du combat des serviteurs à régler au game feeling : `AttackDamage` 15, `AttackRange` 100, `AttackInterval` 1.2 s, `MaxHealth` 100 de `BP_Servant` (instance de `UHealthComponent`) ; `CombatWalkSpeed` 400, `CombatNextDelay` 0.5 s, `EnemyScanInterval` 0.5 s, `FailedEnemyCooldown` 5.0 s (`AServantAIController`) ; `RepathInterval` 1.0 s (`AEnemyAIController`, suivi d'une cible mobile). Le rayon d'arrêt `AttackRange * 0.5` (constante `AcceptanceRadiusFactor`) a été jugé bon par Timéo en PIE
- [ ] Bornes du FOV 60-120 (`SettingsCatalog`) et défaut 100 : placeholders, le GDD n'en fixe pas ; Timéo les a acceptées en spec ; à valider au game feeling

**Ennemis : à traiter à part (pas un prérequis)**
- [ ] À traiter à part, pas un prérequis (Timéo : « ce sera à faire dans le prochain point » ; la 2.2c ne l'a pas traité) : `AEnemyAIController::ChooseTarget` rappelle un `TActorIterator<AActor>` à chaque tick tant qu'aucune cible n'est disponible (coût soldats × acteurs ; remède possible : mémoriser l'heure du prochain essai ; à n'optimiser qu'après mesure)
- [ ] À traiter à part, pas un prérequis : `ExcludedTargets` (`AEnemyAIController`) n'est jamais purgé. Effet nul (la table disparaît avec le contrôleur), noté pour mémoire

**Tests non automatisés (à rejouer à la main si le code concerné change)**
- [ ] Non automatisé (`FTestWorldWrapper` n'a pas de NavMesh) : la récolte et la plantation hors portée, l'annulation en route (champ récolté, planté ou détruit, dé-assignation, `UnPossess` pendant le trajet) et le flag `bWakePending` levé pendant un travail de champ n'ont pas de test automatique. Testés à la main en PIE par Timéo (récolte : 2026-11-13 ; plantation, annulations et flag : date non renseignée) ; à rejouer à la main si le contrôleur change
- [ ] Non automatisé (pas de NavMesh dans les mondes de test) : le déplacement réel des ennemis. Testé à la main en PIE par Timéo le 2026-11-23 : contournement d'obstacle, répartition sur plusieurs destructibles, reprise d'une cible dont le soldat est mort, destructible détruit puis repli sur un autre, pas de cible hors zone, animation de marche sans glissement, transit et drops inchangés, fin de vague après la mort des soldats. **Non testé à la main** : un soldat poussé hors de portée par un autre (relance du déplacement, coups sautés hors de portée), couvert seulement par des Automation Specs avec couture sans NavMesh (C11, C13). À rejouer à la main si le contrôleur ou le spawner changent
- [ ] Non automatisé (pas de NavMesh dans les mondes de test) : le combat réel des serviteurs. Testé à la main en PIE par Timéo le 2026-11-24 dans le scénario global (combat de base, ciblage des serviteurs par les soldats, mort d'un serviteur sans os, récolte, mécaniques impactées), puis re-testé après deux retours (orientation vers l'ennemi, réveil par le scan) : poursuite réelle `MoveToActor` et suivi d'un serviteur mobile par les soldats (`RepathInterval` réel), réservation de champ libérée à la mort d'un serviteur en route, `IsHarvesting` conservé quand un `FieldHarvest` est frappé. **Non couvert par une Spec, ni vu isolément** : la libération de la réservation (claim) d'un soldat sur un serviteur quand celui-ci meurt. À rejouer à la main si `AServantAIController`, `AEnemyAIController` ou `AServant` changent
- [ ] Non automatisé : `CreateWidget` + `AddToPlayerScreen` réels, pause réelle (`IsGamePaused`) et `FInputMode*`, bascule `Visible`/`Collapsed` à l'écran, `WBP` branchés sur le pawn, Tab/Entrée, curseur ; timer de `SetLifeSpan` des drops pendant la pause. Testés à la main en PIE par Timéo le 2026-11-25 ; à rejouer si `ASpooky_SporesPlayerController` ou `UIEntries` changent
- [ ] Non automatisé : le menu des paramètres à l'écran (pause réelle à l'ouverture, `CreateWidget` + `AddToPlayerScreen` réels, slider à l'écran et à la souris : crans de 1 et texte fixe de 99 à 100, FOV visible à la reprise, persistance réelle entre deux lancements, entrée de `UIEntries`). Testés à la main en PIE par Timéo le 2026-11-26 ; à rejouer si `USettingsSubsystem`, `USettingsMenuWidget`, la ligne (`USettingFloatRowWidget`), le FOV du Character ou `UIEntries` changent

**Architecture et drops**
- [ ] `WBP_CauldronProgress` trouve le chaudron par `GetActorOfClass` (un seul chaudron supporté). Timéo : « à voir comment est géré l'affichage plus tard, ce sera à noter pour l'item 15 quand je pourrais construire des chaudrons via la gestion de la base. Pour l'instant on fait l'option A »
- [ ] Divergence assumée : `AResourceDrop::SpawnDrops` (chemin joueur, drops au sol) saute les entrées de `HarvestDrops` sans `DropClass`, `AFieldPlot::ComputeHarvestYield` (chemin serviteur) les compte : un champ sans `DropClass` rapporte donc au serviteur mais rien au joueur. Timéo : « Garde l'état actuel ça me va. De toute façon ça changera bien quand je vais faire l'inventaire. » Disparaît avec l'item 7

**Refactors et nettoyage**
- [ ] `UServantTaskTestListener` (`Tests/`) est compilée dans toutes les configurations (UHT refuse une `UCLASS` sous `#if`) : à revoir si un module de test séparé voit le jour
- [ ] Factoriser le calcul de la cible dans `UGrabComponent` (`GetGrabTargetTransform`)
- [ ] `TObjectPtr` dans `Interaction/`, `Core/Character/` et `Core/Controller/` (`Spooky_SporesPlayerController.h` mélange pointeurs bruts hérités du template, `DefaultMappingContexts`, et `TObjectPtr`) (par domaine, voir `CLAUDE.md` section 3)
- [ ] Nettoyer les assets inutilisés du template (animations `Pistol`...) : branche `chore/`, via le Content Browser
- [ ] `CLAUDE.md` : préciser quels projets doivent être rendus en anglais (CDC pas encore communiqués)

## Limites connues (acceptées)
- **Serviteurs** : le serviteur peut vider les graines du joueur (assumé par Timéo, disparaît avec l'inventaire partagé, item 7) · annulation d'un travail de champ seulement possible avec un déplacement actif (comportement moteur `StopMovement`/`AbortMove`) · la pause après un `Return` réussi avec `bWakePending` levé n'est pas testée en Spec · les serviteurs sont devenus des cibles des soldats (ils meurent sans rendre d'os, `GDD.md` section 2) · la mémoire d'ennemi inatteignable ne retient qu'un seul ennemi (`FailedEnemy`) : deux ennemis inatteignables en même temps font alterner le serviteur entre eux (cas d'un NavMesh mal posé) · le scan d'ennemis tourne toutes les 0,5 s pendant toute l'errance, même sans tâche d'attaque (le callback ressort aussitôt) · l'orientation vers l'ennemi est instantanée, sans lissage
- **Interaction et physique** : Push fusionné dans le grab · un objet tenu peut traverser un obstacle fin si le joueur bouge vite · redressement d'un drop instantané · drop non aligné sur une pente · champignon fracturé en 2 morceaux seulement · couleurs des morceaux visibles uniquement dans l'éditeur
- **Ennemis** : un soldat sans destructible vivant dans la base reste immobile et la vague ne se termine jamais (un seul `UE_LOG(Error)`, pas de filet de chrono : proposition rejetée par Timéo) · si toutes les cibles sont inatteignables (NavMesh mal posé), le soldat attend indéfiniment (un seul `UE_LOG(Error)` distinct) · pas d'évitement entre soldats : la réservation réduit l'empilement sans l'annuler (cas tous réservés) · `DistanceAlongSpline` avance au temps et non à la position réelle du soldat (pré-existant) · cibles des soldats aujourd'hui : les destructibles de la base **et** les serviteurs (`AServant` est un `IDestructible`) dont l'origine est dans la zone de base ; côté destructibles, ce sont les `BP_Harvestable_Stone_Destructible` (Timéo en a ajouté dans `Lvl_Principal` pour le test de 3b ; les `GC_Mushroom` ne sont pas des cibles ; le cœur du joueur n'existe pas encore)
- **Menu des paramètres** : pas d'aperçu du FOV en pause (visible à la reprise, choix accepté par Timéo) · un pas qui ne divise pas la plage rend `Max` inatteignable (non spécifié) · `Border` du menu à taille fixe (600 x 400) sans `ScrollBox`, à revoir quand le menu grossit · largeur minimale 64 du texte de valeur à élargir pour un texte plus long

## Plus tard (visuel, son, idées)
- **Effet de disparition d'un drop expiré** (aujourd'hui `Destroy` silencieux)
- **Réglage du son** (volumes général/musique/effets, sorti de l'item 4 le 2026-11-25) : à ajouter comme entrée de `SettingsCatalog` quand le jeu aura des sons à régler ; un slider ne contrôlerait rien d'audible aujourd'hui
- **Visuel/son** : matériaux réels (champignon, drops, nœuds, `AFieldPlot`) · apparence de l'UI · son et effet au ramassage · rotation/flottement des drops · redressement doux · particules d'impact et tremblement de l'objet frappé · Niagara des sorts · plus de fragments · remplacer le `DrawDebugLine` de `UInteractionComponent` par une surbrillance/réticule · habiller les abords hors zone jouable (`GDD.md` section 3)
- **Idées à rediscuter** : attraction des drops vers le joueur · fusion des drops identiques · repousse des arbres et pierres · sélection de recette au chaudron (dès 2 recettes ; l'UI drag-and-drop attend l'inventaire, items 7-8) · écran de gestion commun à onglets (inventaire, serviteurs) ouvert par Tab, à trancher à l'item 7 (Timéo : « La solution que tu proposes d'avoir des onglets peut être très intéressante. J'y réfléchirai quand je serai à l'inventaire »)
