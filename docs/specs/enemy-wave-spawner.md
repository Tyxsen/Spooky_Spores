# Spec — Spawner de vagues d'ennemis (item 3a)
Item TODO : 3 « Spawner de vagues d'ennemis » | Branche : feature/enemy-wave-spawner | Statut : à valider

## Objectif
Faire apparaître des vagues de `AEnemySoldier` au point 0 de `AEnemyPathSpline` (GDD, « Déclenchement des vagues »). Le chrono entre vagues démarre quand la vague précédente est entièrement tuée : le joueur souffle et reconstruit. 20 vagues par défaut ; après la dernière tuée, la séquence s'arrête (le joueur peut finir le jeu, village vide). Prérequis des items 15/16 et de 3b.

## Comportement
- `AEnemyWaveSpawner` : acteur placé (un par niveau), `EditInstanceOnly` vers `AEnemyPathSpline`. Pas de sous-système (aucun consommateur à servir). Logique en C++, le Blueprint ne fait que configurer.
- **Données** : `TArray<FEnemyWaveEntry> Waves` (`EnemyClass`, `Count`), 20 entrées par défaut. Les valeurs par défaut (classe, comptes) sont des **placeholders** réglés dans l'instance placée.
- **Lancement** : `StartWaves()` démarre la vague 0 immédiatement (touche de test PIE via Blueprint). Si `bAutoStart`, `BeginPlay` arme un timer `FirstWaveDelay` qui appelle `StartWaves()`. `StartWaves()` pendant que ce timer court l'annule et démarre tout de suite ; pendant qu'une séquence tourne, il est ignoré. Après `StopWaves()` ou `OnAllWavesCleared`, il repart de la vague 0.
- **Étalement** : un soldat toutes les `SpawnStagger` (1.5 s), le premier à l'instant du début de vague.
- **Spawn** : `SpawnActorDeferred` → `SetPathSplineActor` → `FinishSpawning`, à la position du point 0, orienté selon la tangente. `AdjustIfPossibleButAlwaysSpawn` (jamais de soldat perdu).
- **Suivi** : le spawner lie `OnDestroyed` de chaque soldat spawné (liste de `TWeakObjectPtr`). Choix : `HandleDeath` appelle `Destroy()`, donc `OnDestroyed` couvre mort et destruction quelconque, sans se déclencher au changement de niveau (contrairement à `OnEndPlay`).
- **Fin de vague** : « vague terminée » = tous les soldats de la vague spawnés **et** aucun vivant. Alors `OnWaveCleared` ; si ce n'était pas la dernière, timer `WaveInterval` (180 s) puis vague suivante ; sinon `OnAllWavesCleared` et arrêt. Un seul chrono actif à la fois, pas de chevauchement.
- Chrono via `FTimerManager` (gelé par la pause du jeu). Ennemis placés à la main : ignorés (jamais liés, ne comptent dans aucune vague).
- `StopWaves()` : annule tous les timers, délie les soldats vivants (ils restent en jeu), n'émet aucun événement ensuite.
- Erreurs de config : spline nulle ou `Waves` vide → `UE_LOG(Error)`, `StartWaves()` ne démarre rien. Entrée à classe nulle → `UE_LOG(Error)`, vague vide. Entrée `Count <= 0` → vague vide. Une vague vide émet `OnWaveStarted` puis `OnWaveCleared` immédiatement.
- `AEnemySoldier::SetPathSplineActor(AEnemyPathSpline*)` : public, C++ seul (pas de `UFUNCTION`).
- **`MoveToLocation` : non migré.** L'ennemi suit la spline en `AddMovementInput` (`documentation-technique.md` §11) ; le glissement venait de `bUseAccelerationForPaths`, propre à `MoveToLocation`.
- **Possession différée, vérifié** dans `Engine/Source/Runtime/Engine/Private/Actor.cpp` : `FinishSpawning` → `PostActorConstruction` → `PostInitializeComponents`, et `APawn::PostInitializeComponents` (`Pawn.cpp`, l.134-159) appelle `SpawnDefaultController()` → `OnPossess`. Le setter appelé avant `FinishSpawning` est donc lu par `OnPossess`. Prérequis non vérifié (Blueprint) : `BP_EnemySoldier.AutoPossessAI = PlacedInWorldOrSpawned`.
- **Ciblage en `Engage` (modifie `AEnemyAIController`)** : `FindNearestDestructible` ne retient que les `IDestructible` vivants dont `GetActorLocation()` est dans la zone de base (`ABaseZone::IsPointInZone`, existante, test 2D tant que `bIgnoreHeight`), le plus proche **du soldat** l'emporte (distance 2D, comme `IsPointInZone` et la mesure d'approche), sans limite de rayon. `TargetSearchRadius` supprimé. Retargeting : cible détruite **ou à 0 PV** (un destructible mort n'est pas forcément détruit) → nouvelle recherche au tick suivant.
- **Zone** : `UBaseZoneSubsystem::GetBaseZone()` appelé à chaque recherche, jamais mis en cache (nullptr si pas encore enregistrée). Aucune zone, ou zone sans destructible vivant → pas de cible.
- **Pas de cible = erreur de configuration visible, pas de filet** : soldat en `Engage` sans cible → `UE_LOG(Error)` **une seule fois par soldat** (booléen membre du contrôleur, pas à chaque tick) ; sinon comportement inchangé (immobile, vivant). `WaveMaxDuration` rejeté par Timéo : si ça arrive, la vague ne finit pas, c'est à corriger dans la config (zone/destructibles).

## Critères d'acceptation
- [ ] Un soldat spawné a `GetPathSplineActor()` non nul dès sa première frame, apparaît au point 0 de la spline et son contrôleur avance dessus (setter lu par `OnPossess`).
- [ ] Une vague de N produit exactement N soldats, le k-ième à k×`SpawnStagger` (1er à t=0).
- [ ] Tuer tous les soldats d'une vague émet `OnWaveCleared` une fois ; `WaveInterval` plus tard (pas avant), `OnWaveStarted` et `GetWaveIndex()` +1. Si tous les soldats déjà spawnés meurent pendant l'étalement, pas de `OnWaveCleared` avant que le dernier soit spawné puis tué.
- [ ] Tuer le dernier soldat de la dernière vague émet `OnAllWavesCleared`, puis plus aucun spawn ni événement.
- [ ] `StartWaves()` deux fois d'affilée : une seule séquence ; appelé pendant `FirstWaveDelay`, démarre immédiatement et le timer d'auto-start ne relance rien.
- [ ] `StopWaves()` pendant l'étalement : plus aucun spawn ni événement (même à la mort des soldats déjà là, qui restent vivants).
- [ ] Spline nulle ou `Waves` vide : aucun spawn, `OnWaveStarted` jamais émis ; classe nulle ou `Count = 0` : vague vide, séquence continue.
- [ ] Ciblage : avec deux destructibles vivants dans la base, le soldat attaque le plus proche **de lui** (même à > 2000 u) ; un destructible plus proche mais hors zone, ou mort, est ignoré.
- [ ] Retargeting : quand la cible meurt, le soldat attaque le destructible vivant restant le plus proche de lui, dans la zone.
- [ ] Sans zone enregistrée, ou zone sans destructible vivant (ou seulement hors zone) : le soldat en `Engage` reste immobile et vivant, exactement 1 `UE_LOG(Error)` sur N ticks ; une zone enregistrée après coup est prise en compte (pas de nullptr en cache).

## API exposée
- `USTRUCT(BlueprintType) FEnemyWaveEntry` : `UPROPERTY(EditAnywhere, BlueprintReadWrite) TSubclassOf<AEnemySoldier> EnemyClass`, `int32 Count = 0`.
- `UFUNCTION(BlueprintCallable, Category="Waves") void StartWaves();` `void StopWaves();`
- `UFUNCTION(BlueprintPure, Category="Waves") int32 GetWaveIndex() const;` index 0-based de la vague en cours ou dernière lancée, `-1` avant la première.
- `UPROPERTY(BlueprintAssignable) FOnWaveStarted OnWaveStarted(int32 WaveIndex)`, `FOnWaveCleared OnWaveCleared(int32 WaveIndex)`, `FOnAllWavesCleared OnAllWavesCleared()` (dynamic multicast).
- `AEnemySoldier::SetPathSplineActor(AEnemyPathSpline*)` public C++.

## Valeurs réglables
- `Waves` : 20 entrées, placeholders (comptes à régler dans l'instance).
- `WaveInterval` : 180 s (donné par Timéo, à réécrire au game feeling), mesuré de la fin de vague au début de la suivante.
- `SpawnStagger` : 1.5 s.
- `bAutoStart` : false (aucun lancement au PIE ; le tutoriel appelle `StartWaves()` quand il en a besoin, décision de Timéo). `FirstWaveDelay` : 180 s, **placeholder non validé**.
- Chrono : `StartWaves()` lance la vague ; ensuite `WaveInterval` démarre à chaque vague entièrement tuée. Pas de `bAutoChainWaves` ni `StartNextWave()` (rejetés).
- `TargetSearchRadius` : supprimé.
- `PathSplineActor` : `EditInstanceOnly`, nul par défaut.

## Cas limites
- Soldat détruit pendant `FinishSpawning`/au spawn (collision, `OnDestroyed` avant la liaison) : lier juste après `FinishSpawning`, tester `IsValid` ; un soldat déjà détruit compte comme mort.
- `StopWaves()` pendant un callback d'événement (ex. depuis `OnWaveCleared`) : aucun timer ne doit rester armé.
- Soldat placé à la main : comportement inchangé, n'influence aucune vague (vérifier en PIE).
- Destructible à cheval sur la limite : seule sa position (`GetActorLocation`) compte.
- Une propriété modifiée sur l'instance placée ignore les nouveaux défauts C++ : vérifier l'instance du niveau.

## Périmètre de fichiers
- À créer : `Enemies/EnemyWaveSpawner.h/.cpp`, `Tests/EnemyWaveSpawnerSpec.cpp` (monde de test sans NavMesh, propriétés réglées par réflexion comme `ServantWanderTestHelpers.h`).
- Aussi à créer : `Tests/EnemyTargetingWorldSpec.cpp` (monde de test avec `ABaseZone` + destructibles ; nom aligné sur `*WorldSpec.cpp`, helpers de `BaseZoneTestHelpers.h` réutilisables). Observer le comportement (PV, position, log capturé), `FindNearestDestructible` reste privée.
- À modifier : `Enemies/EnemySoldier.h/.cpp` (`SetPathSplineActor`), `Enemies/EnemyAIController.h/.cpp` (ciblage dans la zone, log unique, suppression de `TargetSearchRadius`). Seule référence trouvée : le `.h`/`.cpp` du contrôleur et `documentation-technique.md` l.159 ; aucun test existant n'y touche.
- Points chauds : aucun côté C++. Intégration : placer le spawner dans `Lvl_Principal.umap` (point chaud ; présence d'une `AEnemyPathSpline` dans le niveau non vérifiée) ; contrôler `BP_EnemySoldier` (une valeur `TargetSearchRadius` surchargée dans le BP ou une instance placée devient orpheline : à signaler/réinitialiser, `bp-integration-planner`), vérifier que la `ABaseZone` et les destructibles du niveau existent, et créer la touche de test PIE (Blueprint).

## Docs à mettre à jour en fin de branche (`consistency-keeper`)
- `TODO.md` ligne 16 (zone de fin) : « seuil de vagues survécues à fixer par playtest » est contredit par la décision de Timéo (20 vagues, tout le monde mort, accès à la fin ouvert). La ligne d'errance « attaque/défense : item 3 » devient 3b ; item 3 → 3a.
- `GDD.md` (ligne 155, déblocage par « nombre de vagues survécues ») : même contradiction, à signaler à Timéo (pas de modification par cet agent).
- `documentation-technique.md` §11 (l.159) : ciblage `Engage` dans la zone de base, plus de rayon `TargetSearchRadius`, log d'erreur unique ; remplacer « Un futur spawner devra… » par le spawner réel ; ajouter `EnemyWaveSpawner` à la liste de `Enemies/` (§2).

## Hors périmètre
- 3b : serviteurs attaque/défense, règle d'errance, détection d'ennemis vivants.
- Zone de fin (item 16) et définition de « vague survécue » ; coûts de construction (15).
- Puissance, nouveaux types d'ennemis, équipement visuel, tutoriel, musique, `MoveToLocation`.
- Défaite/victoire de run (appel futur à `StopWaves()`).

## Questions restantes pour Timéo
1. **Délai avant la 1re vague** : placeholder 180 s, non validé (`bAutoStart = false`, donc sans effet tant que le tutoriel appelle `StartWaves()`). À fixer plus tard si tu actives l'auto-start.
2. **Interprétation à confirmer** : « le plus proche » = le plus proche du **soldat**, parmi les destructibles dans la base (pas le plus proche du joueur). Corrige-moi si tu voulais autre chose.
