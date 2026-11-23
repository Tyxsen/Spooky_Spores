# TODO — Spooky_Spores

Seul le reste à faire figure ici. Le déjà fait est dans `git log` et `documentation-technique.md`. Pas de découpage par semaines : on déroule la file (règles : `CLAUDE.md` section 9). Release GitHub à chaque étape significative (`v0.2`, `v0.3`...).

## Prochaine session
- **État** : 3b (ennemis : déplacement NavMesh en `Engage` + réservation souple des cibles) terminée et validée en PIE par Timéo le 2026-11-23.
- **Prochaine tâche** : item 2.2c, rôle « attaque/défense » des serviteurs, avec la règle d'errance de Timéo (item 2, plus bas).
- **À savoir avant de commencer** :
  - Penser à ajouter le fait que les serviteurs sont destructibles avec un certain nombre de points de vie pour pouvoir se faire attaquer par les ennemis.
  - la détection des ennemis vivants est à créer (rien n'existe côté serviteur) ;
  - le ciblage partagé et le déplacement des ennemis sont stabilisés (3b) ;
  - les mondes de test n'ont pas de NavMesh : prévoir une couture de test comme pour 3b (méthode virtuelle protégée autour de `MoveToLocation`, voir `documentation-technique.md` section 11) ;
  - deux dettes « à regarder en passant » sont liées à la 2.2c, sans être un prérequis (section Dette technique : `TActorIterator` par tick dans `ChooseTarget`, `ExcludedTargets` jamais purgé).
- **Rappels** : la dette « Avant toute Release » (section Dette technique) ; points chauds à ne pas modifier en parallèle : `EResourceType`, `*.Build.cs`, `Default*.ini`, `TODO.md`, `CLAUDE.md`, `documentation-technique.md`, `Lvl_Principal.umap` (`CLAUDE.md` section 10).

## En cours
(rien de codé : la branche de 3b attend sa fusion dans `dev`, voir « Prochaine session »)

## File d'attente (dans l'ordre)
Les numéros sont des identifiants stables (cités ailleurs : « item 6 », « items 7-8 »...) : les items faits sont retirés sans renuméroter.
2. **Assignation des tâches aux serviteurs** : menu à deux colonnes (serviteurs non assignés / catégories « attaque-défense », « récolte de champ », potentiellement d'autres si besoin ou bonnes idées). La récolte automatique découle de l'assignation, pas d'un comportement indépendant. Ferme la boucle principale (résurrection, pose, errance : faits).
   - [x] 2.1 Menu · [x] 2.2a Récolte de champ (le serviteur assigné `FieldHarvest` récolte les champs `Grown` et crédite directement le compteur du joueur) · [x] 2.2b Replantage (le serviteur récolte et replante sur place ; crédit puis dépense, le reste des graines reste au compteur du joueur)
   - [ ] 2.2c Rôle « attaque/défense » (le spawner d'ennemis existe). Applique la règle d'errance de Timéo (ci-dessous) : le serviteur quitte l'errance seulement s'il y a des ennemis vivants à tuer, et y revient quand il n'y en a plus. Demande la détection des ennemis vivants. 3b faite : le ciblage partagé et le déplacement des ennemis sont stabilisés.
   - Règle d'errance (Timéo, pour 2.2 et attaque/défense ; récolte : faite en 2.2a ; attaque/défense : 2.2c) : « Alors en soit l'assignation des tâches ne doit pas rééllement enlever l'état d'errance, il l'enlève seulement si il y a des fieldPlot à gérer dans la base ou si il y a des ennemis à tuer. Dans le cas où il y a plus de champ ou plus d'ennemis, le serviteur se remet dans un êtat d'errance même en étant assigné, dans l'attente de nouvelles choses à faire. »
4. Menu de paramètres : son, FOV, remappage des touches (Enhanced Input, Player Mappable Keys)
5. Sauvegarde (voir plus bas)
6. UI hors du Character (PlayerController/HUD) + despawn des drops au sol après délai
7. Vrai inventaire (stacks, slots, UI liste)
   - Cible de Timéo : « un objet dans la base à côté du coeur du joueur qui va en fait être un inventaire virtuel à distance qui sera en fait mon inventaire et celui des serviteurs, une sorte d'inventaire partagé en remote dans lequel les serviteurs pourront piocher pour replanter ». Décision de récolte : « quand c'est le serviteur qui récolte, les drops n'apparaissent pas mais au contraire que ça s'ajoute directement dans l'inventaire partagé ».
   - Stockage provisoire (décidé le 2026-11-09) : c'est le compteur du joueur 0 (`UResourceCounterComponent`), la Necrospore y est une ressource comptée. Chaque accès reste isolé à un endroit précis, pour que le passage au vrai inventaire ne touche que ceux-là : pose de Necrospore, `DoPlaceServant` (`TrySpendResource`) ; récolte serviteur, dans `AServantAIController` (même fichier), `FindHarvestStorage` (résolution), `HarvestAndDeposit` (crédit), `TryPlantOnField` (dépense des graines) et `WatchStorage` (abonnement à `OnResourceChanged`).
8. UI drag-and-drop + barre de raccourcis (9 emplacements). Le menu des serviteurs passe de la sélection par clic au glisser-déposer (seul le widget change, l'API C++ reste)
9. Objets équipables + contrôles contextuels
10. Roue de sorts (clic droit maintenu) + mana généralisé (4 emplacements, recharge ~5 s)
11. Sorts 2 et 3
12. Sort 4
13. Vue de gestion top-down (pause à l'entrée) — **indissociable de l'item 14**. Reprendre la pause du menu des serviteurs (fonction privée du Character), premier consommateur
14. Construction façon city-builder (grille, placement, rotation, déplacement, aperçu vert/rouge). Cases du chemin vers le village verrouillées en permanence (`GDD.md` section 3). Indissociable du 13. L'agrandissement de la base doit mettre à jour `BP_BaseZone`.
15. Construction ↔ ressources (coût, effets des bâtiments)
16. Zone de fin : trigger box le long du chemin, accessible après un seuil de vagues survécues (à fixer par playtest) **et** accès garanti quand `AEnemyWaveSpawner::OnAllWavesCleared` est émis (20 vagues tuées par défaut, village vide : le joueur a gagné par défaut, même sans la puissance nécessaire, `GDD.md` section 4), indicateur visuel (`GDD.md` section 2)
17. Victoire + clôture narrative : à l'entrée de la zone de fin, choix terminer / continuer ; cinématique à variantes selon le nombre de serviteurs (`GDD.md` section 2)

## Sauvegarde (à concevoir avant la progression)
Positions des objets saisissables · nœuds récoltés et drops au sol · `bHasTriggered` des `AStoryTrigger` · ressources du `UResourceCounterComponent` · plus tard : plantations, serviteurs, potions, état de la base. Données externes : c'est ici qu'on durcit (NaN, valeurs hors bornes).

## Dette technique

**Avant toute Release**
- [ ] **Avant toute Release** : supprimer `AEnemySoldier::DebugKill` et sa touche dans `BP_FirstPersonCharacter`, ainsi que la touche de test `StartWaves` (même Blueprint) ; supprimer le nœud de test `Add Resource (Necrospore)` du `BeginPlay` de `BP_FirstPersonCharacter` ; remettre `Hidden In Game` à `true` sur `ZoneBox` de `BP_BaseZone`

**À régler (valeurs et réglages)**
- [ ] Placeholders du spawner de vagues à régler (instance dans `Lvl_Principal`) : `Count` des 20 entrées de `Waves` (3 chacune aujourd'hui) ; `FirstWaveDelay` 180 s non validé ; `WaveInterval` 180 s à réécrire au game feeling

**Ennemis : à regarder en passant avec la 2.2c (pas un prérequis)**
- [ ] À regarder en passant avec la 2.2c, pas un prérequis (Timéo : « ce sera à faire dans le prochain point ») : `AEnemyAIController::ChooseTarget` rappelle un `TActorIterator<AActor>` à chaque tick tant qu'aucune cible n'est disponible (coût soldats × acteurs ; remède possible : mémoriser l'heure du prochain essai ; à n'optimiser qu'après mesure)
- [ ] À regarder en passant avec la 2.2c, pas un prérequis : `ExcludedTargets` (`AEnemyAIController`) n'est jamais purgé. Effet nul (la table disparaît avec le contrôleur), noté pour mémoire

**Tests non automatisés (à rejouer à la main si le code concerné change)**
- [ ] Non automatisé (`FTestWorldWrapper` n'a pas de NavMesh) : la récolte et la plantation hors portée, l'annulation en route (champ récolté, planté ou détruit, dé-assignation, `UnPossess` pendant le trajet) et le flag `bWakePending` levé pendant un travail de champ n'ont pas de test automatique. Testés à la main en PIE par Timéo (récolte : 2026-11-13 ; plantation, annulations et flag : date non renseignée) ; à rejouer à la main si le contrôleur change
- [ ] Non automatisé (pas de NavMesh dans les mondes de test) : le déplacement réel des ennemis. Testé à la main en PIE par Timéo le 2026-11-23 : contournement d'obstacle, répartition sur plusieurs destructibles, reprise d'une cible dont le soldat est mort, destructible détruit puis repli sur un autre, pas de cible hors zone, animation de marche sans glissement, transit et drops inchangés, fin de vague après la mort des soldats. **Non testé à la main** : un soldat poussé hors de portée par un autre (relance du déplacement, coups sautés hors de portée), couvert seulement par des Automation Specs avec couture sans NavMesh (C11, C13). À rejouer à la main si le contrôleur ou le spawner changent

**Architecture et drops**
- [ ] Déplacer la création de l'UI hors du personnage (item 6), y compris le menu des serviteurs et sa pause
- [ ] Faire disparaître les drops restés au sol après un délai (drops du joueur : Timéo « Oui quand c'est le joueur je veux des drops quand même. », item 6)
- [ ] Divergence assumée : `AResourceDrop::SpawnDrops` (chemin joueur, drops au sol) saute les entrées de `HarvestDrops` sans `DropClass`, `AFieldPlot::ComputeHarvestYield` (chemin serviteur) les compte : un champ sans `DropClass` rapporte donc au serviteur mais rien au joueur. Timéo : « Garde l'état actuel ça me va. De toute façon ça changera bien quand je vais faire l'inventaire. » Disparaît avec l'item 7

**Refactors et nettoyage**
- [ ] `UServantTaskTestListener` (`Tests/`) est compilée dans toutes les configurations (UHT refuse une `UCLASS` sous `#if`) : à revoir si un module de test séparé voit le jour
- [ ] Factoriser le calcul de la cible dans `UGrabComponent` (`GetGrabTargetTransform`)
- [ ] `TObjectPtr` dans `Interaction/` et `Core/Character/` (par domaine, voir `CLAUDE.md` section 3)
- [ ] Nettoyer les assets inutilisés du template (animations `Pistol`...) : branche `chore/`, via le Content Browser
- [ ] `CLAUDE.md` : préciser quels projets doivent être rendus en anglais (CDC pas encore communiqués)

## Limites connues (acceptées)
- **Serviteurs** : le serviteur peut vider les graines du joueur (assumé par Timéo, disparaît avec l'inventaire partagé, item 7) · annulation d'un travail de champ seulement possible avec un déplacement actif (comportement moteur `StopMovement`/`AbortMove`) · la pause après un `Return` réussi avec `bWakePending` levé n'est pas testée en Spec
- **Interaction et physique** : Push fusionné dans le grab · un objet tenu peut traverser un obstacle fin si le joueur bouge vite · redressement d'un drop instantané · drop non aligné sur une pente · champignon fracturé en 2 morceaux seulement · couleurs des morceaux visibles uniquement dans l'éditeur
- **Ennemis** : un soldat sans destructible vivant dans la base reste immobile et la vague ne se termine jamais (un seul `UE_LOG(Error)`, pas de filet de chrono : proposition rejetée par Timéo) · si toutes les cibles sont inatteignables (NavMesh mal posé), le soldat attend indéfiniment (un seul `UE_LOG(Error)` distinct) · pas d'évitement entre soldats : la réservation réduit l'empilement sans l'annuler (cas tous réservés) · `DistanceAlongSpline` avance au temps et non à la position réelle du soldat (pré-existant) · cibles réelles des soldats aujourd'hui : les `BP_Harvestable_Stone_Destructible` de la base (Timéo en a ajouté dans `Lvl_Principal` pour le test de 3b ; les `GC_Mushroom` ne sont pas des cibles ; le cœur du joueur n'existe pas encore)

## Plus tard (visuel, son, idées)
- **Visuel/son** : matériaux réels (champignon, drops, nœuds, `AFieldPlot`) · apparence de l'UI · son et effet au ramassage · rotation/flottement des drops · redressement doux · particules d'impact et tremblement de l'objet frappé · Niagara des sorts · plus de fragments · remplacer le `DrawDebugLine` de `UInteractionComponent` par une surbrillance/réticule · habiller les abords hors zone jouable (`GDD.md` section 3)
- **Idées à rediscuter** : attraction des drops vers le joueur · fusion des drops identiques · repousse des arbres et pierres · sélection de recette au chaudron (dès 2 recettes ; l'UI drag-and-drop attend l'inventaire, items 7-8) · écran de gestion commun à onglets (inventaire, serviteurs) ouvert par Tab, à trancher à l'item 7 (Timéo : « La solution que tu proposes d'avoir des onglets peut être très intéressante. J'y réfléchirai quand je serai à l'inventaire »)
