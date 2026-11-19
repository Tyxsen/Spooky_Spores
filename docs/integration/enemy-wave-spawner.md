# Checklist d'intégration : spawner de vagues (item 3a)
Rédigée éditeur FERMÉ, C++ non compilé : tout état de l'éditeur est « à vérifier à l'ouverture ». Seuls 4-5 pas sont [MCP] et l'essentiel est du contrôle/câblage : lancer `bp-integrator` n'est utile que pour le placement et les valeurs ; le reste est plus rapide à la main.

## 1. Pré-requis
- Branche `feature/enemy-wave-spawner` (jamais `dev`/`main`) ; arbre propre.
- Classes à retrouver dans l'éditeur : `AEnemyWaveSpawner`, `FEnemyWaveEntry`, `AEnemySoldier` (`SetPathSplineActor`, C++ seul), `AEnemyAIController`, `AEnemyPathSpline`, `ABaseZone`.
- [HUMAIN] Éditeur fermé, régénérer les fichiers de projet (clic droit `.uproject`, nouveaux `.h/.cpp`), Rebuild complet dans Rider (pas Live Coding), lancer les Automation Specs (filtre `SpookySpores`, `EnemyWaveSpawnerSpec`, `EnemyTargetingWorldSpec`) : toutes vertes. Puis rouvrir sur la branche ; `system_control.get_project_settings` doit répondre `Spooky_Spores`.
- Assets : `/Game/Spooky_Spores/Placeables/Enemies/BP_EnemySoldier`, `BP_EnemyPathSpline`, `Placeables/Base/BP_BaseZone`, `Placeables/Destructibles/GC_Mushroom`, `Core/Character/BP_FirstPersonCharacter`.

## 2. Contrôles avant toute écriture (lecture seule)
1. [MCP] `BP_EnemySoldier` : `AutoPossessAI` = `PlacedInWorldOrSpawned`. Attendu : cette valeur. Sinon (défaut moteur `PlacedInWorld`) aucun soldat spawné n'est possédé : il reste immobile, silencieusement. Correction = étape 6.
2. [MCP] `Lvl_Principal` : une `AEnemyPathSpline` (BP_EnemyPathSpline) existe. Relever son nom et le tracé (point 0 = départ, dernier point = base). Si absente : STOP, la placer est une décision de level design (Timéo).
3. [MCP] `Lvl_Principal` : une `ABaseZone` existe (une seule : la seconde est ignorée avec log d'erreur) et au moins un acteur `IDestructible` (ex. `GC_Mushroom`) a son **origine** (`GetActorLocation`) dans la zone, vivant. Relever leurs positions. Sinon : STOP, les soldats n'auront pas de cible.
4. [MCP] `TargetSearchRadius` : chercher une valeur surchargée sur `BP_EnemySoldier` et sur les instances d'`AEnemySoldier` placées. La propriété n'existe plus en C++ : une valeur orpheline est ignorée, seul un avertissement de chargement la trahit. Les lister ; la suppression est [HUMAIN] (Resave / Content Browser, jamais par script).
5. [MCP] Relever sur `BP_EnemySoldier` `AttackRange`, `AttackInterval` et sur les soldats placés (une valeur d'instance masque le BP).

## 3. Écriture (ordre à respecter)
6. [MCP] Si le contrôle 1 a échoué : `BP_EnemySoldier.AutoPossessAI = PlacedInWorldOrSpawned`. Relire : valeur appliquée.
7. [MCP] Créer `/Game/Spooky_Spores/Placeables/Enemies/BP_EnemyWaveSpawner`, enfant de `EnemyWaveSpawner` (config seulement, aucun graphe). Attendu : classe parente lisible = `AEnemyWaveSpawner`, 20 entrées `Waves` de `Count` 3, `WaveInterval` 180, `SpawnStagger` 1.5, `bAutoStart` false.
8. [MCP] **Point chaud `Lvl_Principal.umap` (aucune autre branche ne doit le toucher)** : placer UN `BP_EnemyWaveSpawner` (le spawner est invisible, position libre ; le placer près de la spline). Attendu : un seul `AEnemyWaveSpawner` dans le niveau (`inspect.find_by_class`).
9. [MCP] Sur l'instance placée : `PathSplineActor` = l'`AEnemyPathSpline` du contrôle 2 (`EditInstanceOnly` : impossible dans le BP). Attendu : référence non nulle relue sur l'instance.
10. [HUMAIN] `Waves` (20 entrées) : `EnemyClass` = `BP_EnemySoldier` sur CHAQUE entrée (nulle par défaut : sinon chaque vague loggue une erreur et reste vide) ; `Count` : les 3 sont des placeholders, valeurs à décider par Timéo (suggestion pour un premier test : vague 0 à 2 ou 3). Pas étiqueté [MCP] : l'écriture d'un tableau de structs n'est pas garantie couverte. Attendu : 20 entrées, aucune classe vide.
11. [MCP] Vérifier sur l'instance placée (pas seulement le BP) : `WaveInterval` 180, `SpawnStagger` 1.5, `bAutoStart` false. Une valeur d'instance modifiée ignore les défauts C++ : si elle diffère, la réinitialiser (« Reset to Default », [HUMAIN]) .
12. [HUMAIN] Touche de test dans `BP_FirstPersonCharacter` (Event Graph, logique : jamais [MCP]) : `Get Actor Of Class(EnemyWaveSpawner)` (un seul spawner par niveau, donc sans ambiguïté) puis `Start Waves`. Touche : **O** par défaut (à confirmer libre dans `IMC_Default` ; déjà prises : ZQSD, E, Espace, Maj, L, Entrée, Tab, clics, la touche de `DebugKill`). Jamais `²`, Échap, ni les F-keys du PIE (F8 = éjection). Nœud de DEBUG : à ajouter à la ligne « Avant toute Release » de `TODO.md` (`consistency-keeper`) ; un branchement `Input Action` + `IMC_Default` est plus lourd, inutile pour du debug.
13. [HUMAIN] Compiler `BP_FirstPersonCharacter`, sauvegarder les assets et `Lvl_Principal`.

## 4. Point de spawn
Position = `GetLocationAtSplinePoint(0)` **telle quelle** (centre de la capsule), sans décalage de hauteur. Si le point 0 est posé au sol, la capsule est à moitié enfoncée ; `AdjustIfPossibleButAlwaysSpawn` tente de la remonter, sinon le soldat apparaît quand même (peut tomber/coincer). À vérifier en lisant le Z du point 0 : il doit être ≈ Z sol + demi-hauteur de capsule de `BP_EnemySoldier`. À corriger en déplaçant le point 0 de la spline ([HUMAIN], décision level design), pas dans le spawner.

## 5. Test PIE (Timéo)
1. Lancer, appuyer sur la touche : un soldat à t=0, puis un toutes les 1.5 s au point 0, orientés vers le point 1, et qui avancent sur la spline (preuve de la possession).
2. Les tuer (`DebugKill` ou sort) : après le DERNIER de la vague, `WaveInterval` (180 s ; réduire temporairement pour tester, puis rétablir) repart, pas avant ; la vague 1 démarre.
3. Un soldat arrivé en `Engage` cible le destructible de la base le plus proche DE LUI ; détruire sa cible : il rebascule sur le plus proche restant. Un destructible plus proche mais hors zone est ignoré.
4. Un soldat posé à la main fonctionne comme avant et ne compte dans aucune vague.
5. Cas d'erreur : retirer/déplacer tous les destructibles hors zone (temporairement) : le soldat reste immobile et vivant, UN seul log `Error` « found no living destructible » par soldat dans l'Output Log. Annuler la manip.
6. Touche pressée deux fois : une seule séquence. Pause (Tab) : le chrono se fige.
Mécaniques impactées par le diff : ciblage `Engage`/attaque des soldats, `DebugKill`, destruction des destructibles (butin). Pas d'autre test.

## 6. Risques / non vérifiables
- Rien lu dans l'éditeur : noms de BP, valeurs `AutoPossessAI`, présence spline/zone/destructibles, touche libre : tous supposés.
- Écriture de `Waves` en [HUMAIN] par prudence ; `Lvl_Principal` modifié : un seul auteur à la fois.
- Retrait du `TargetSearchRadius` : valeur orpheline possible (étape 4).
- Spawn au sol (section 4) ; soldat sans cible = vague qui ne finit jamais (choix assumé, pas de filet).
- Nœud de debug (touche) à retirer avant Release.
