# Spec — Ennemis : déplacement NavMesh en Engage et réservation souple des cibles
Item TODO : 3b | Branche : feature/enemy-navmesh-reservation | Statut : décisions de Timéo intégrées, en attente du go explicite

## Objectif
Les soldats contournent les obstacles (arbre) au lieu de s'y coincer, et se répartissent sur les destructibles au lieu de s'empiler à 4 sur le même. Lève la limite « un soldat coincé bloque la fin de vague ». Dépendance : spawner d'ennemis (3a) mergé. Hors périmètre : rôle attaque/défense des serviteurs (2.2c).

## Comportement
- `Transit` inchangé (`AddMovementInput` le long de la spline, pas de NavMesh). Seul `Engage` passe à `MoveToLocation`.
- **Choix de cible** (remplace `FindNearestDestructible`) : candidats = destructibles vivants dont l'origine est dans la zone (inchangé). Cible libre la plus proche en 2D ; sinon la moins occupée (nombre de réservations) ; égalité : la plus proche en 2D.
- **Réservation** : une seule par soldat, posée au choix. Libérée : mort ou destruction du soldat (`OnUnPossess`/`EndPlay` du contrôleur), mort ou destruction de la cible, changement de cible (poser une nouvelle réservation remplace l'ancienne). Pas de rechoix périodique tant que la cible vit.
- **Erreurs** : aucun candidat vivant dans la zone = log `Error` existant, une fois par soldat. « Tous réservés » = cas normal, le soldat rejoint la moins occupée, aucun log.
- **Déplacement** : cible projetée sur le NavMesh avec `TargetProjectionExtent` (explicite, comme `HarvestProjectionExtent` du serviteur), puis `MoveToLocation(..., bStopOnOverlap = true, bUsePathfinding = true, bProjectDestinationToNavigation = false, bCanStrafe = false)`. Une seule requête tant qu'un déplacement est en cours (pas par tick). Un soldat hors de portée **sans déplacement en cours** (ex. arrivé puis poussé hors de portée par un autre soldat) relance une requête vers sa cible, sans changer de cible ni de réservation.
- **Arrivée** : le test d'attaque actuel (point le plus proche de la collision, moins rayon de capsule, 2D, `<= AttackRange`) reste l'autorité, évalué au `Tick`. Dès qu'il est vrai : `StopMovement` (état « en déplacement » remis à faux **avant** l'appel, `OnMoveCompleted` étant synchrone), puis timer d'attaque. `AcceptanceRadius` n'inclut pas le rayon de capsule (`bStopOnOverlap`). **Un coup n'est appliqué que si le soldat est à portée au moment du coup** (même test de portée) : hors de portée (ex. poussé), le coup est sauté, le timer continue et les coups reprennent au retour à portée.
- **Échec de déplacement** (`Failed`, `Blocked`, `AlreadyAtGoal` ou `Success` hors portée, y compris synchrone) : la cible est exclue pour ce soldat pendant `UnreachableRetryDelay`, sa réservation libérée, une autre cible est choisie. Aucun repli en ligne droite. Si toutes sont exclues : le soldat attend (immobile), un `Error` distinct « aucune cible atteignable » une fois par soldat, nouvel essai après le délai.
- `OnMoveCompleted` ignore les fins d'une requête qu'il n'attend plus (cible changée, soldat à portée).
- `AEnemySoldier` : `bUseAccelerationForPaths = true` (sinon glissement sans cycle de marche dans l'Anim Blueprint).
- Choix techniques : registre = `UWorldSubsystem` (modèle `UBaseZoneSubsystem`, jamais `TActorIterator`) ; table `contrôleur -> cible` en pointeurs faibles (une réservation par soldat, nettoyage naturel des morts) ; couture de test = `RequestMoveToTarget` virtuelle protégée (les mondes de test n'ont pas de NavMesh) ; aucun changement de `Default Query Extent` global (extension passée explicitement).

## Critères d'acceptation
- [ ] C1 Deux soldats, cibles A (proche) et B (loin) : le premier choisit A ; le second choisit B même si A est plus proche de lui.
- [ ] C2 Trois soldats, deux cibles : les 2 premiers prennent chacun une cible (1 et 1), le 3e rejoint la plus proche des deux (égalité d'occupation) ; avec comptes 2 et 1, un nouveau soldat rejoint celle à 1, même plus loin ; aucun log d'erreur.
- [ ] C3 Aucun destructible vivant dans la zone (ou zone absente) : exactement 1 log `Error` existant par soldat, aucune réservation. Cas « tous réservés » : 0 erreur.
- [ ] C4 Un soldat détruit (mort ou `Destroy`) libère sa réservation : `GetClaimCount` de sa cible retombe à 0 et un nouveau soldat la prend comme cible libre.
- [ ] C5 Cible tuée (`IsDead` mais encore présente) ou détruite : le soldat prend la plus proche libre, `GetClaimCount` ancienne = 0, nouvelle = 1.
- [ ] C6 À tout instant, la somme des `GetClaimCount` des destructibles = nombre de soldats en `Engage` ayant une cible.
- [ ] C7 Avec la couture de test : exactement une requête de déplacement par cible choisie, vers cette cible, et aucune sur 100 ticks suivants.
- [ ] C8 Soldat déjà à portée au choix : il attaque (timer actif) sans requête de déplacement ; une requête en cours est arrêtée à l'entrée en portée.
- [ ] C9 Contrôleur de production sans NavMesh : la requête échoue, la réservation est libérée, la cible est exclue, 1 seul `Error` « aucune cible atteignable », nouvel essai après `UnreachableRetryDelay` (compté en ticks de monde).
- [ ] C10 `AEnemySoldier` (CDO) : `bUseAccelerationForPaths` vrai.
- [ ] C11 Soldat arrivé à portée puis déplacé hors de portée (sans déplacement en cours) : une nouvelle requête part vers la même cible, `GetCurrentTarget()` et `GetClaimCount` inchangés ; pas de nouvelle requête tant que le déplacement relancé est en cours.
- [ ] C13 Soldat à portée dont le timer d'attaque est actif, puis déplacé hors de portée : aucun dégât sur la cible pendant qu'il est hors de portée ; les dégâts reprennent une fois revenu à portée. Soldat à portée : les dégâts continuent à l'intervalle normal.
- [ ] C12 Fin de déplacement signalée de façon synchrone pendant `RequestMoveToTarget` (`Blocked`) : exactement une exclusion, 0 réservation, 1 seul `Error`. Libération de la réservation aussi quand le contrôleur survit à son pawn (`UnPossess` d'un soldat vivant).

## API exposée
- `UEnemyTargetSubsystem : UWorldSubsystem` (C++ seul, pas de Blueprint) : `void SetClaim(const AController* Claimant, AActor* Target)` (remplace l'ancienne) ; `void ReleaseClaim(const AController* Claimant)` ; `int32 GetClaimCount(const AActor* Target) const` (ne compte que contrôleur et cible valides) ; `AActor* PickTarget(const AController* Claimant, const FVector& Origin, const TArray<AActor*>& Candidates) const` (règle libre/moins occupée ; la réservation du demandeur n'est pas comptée).
- `AEnemyAIController` : `AActor* GetCurrentTarget() const` (public, lecture seule, pour les tests) ; `virtual EPathFollowingRequestResult::Type RequestMoveToTarget(AActor* Target)` (protégée, projection + `MoveToLocation`) ; `virtual void OnMoveCompleted(FAIRequestID, const FPathFollowingResult&) override`.
- `UPROPERTY(EditAnywhere, Category = "Movement|Navigation")` sur le contrôleur : valeurs ci-dessous.

## Valeurs réglables
`TargetProjectionExtent` : `FVector(300, 300, 750)` (à ajuster en PIE ; trop petit = échec silencieux près d'un obstacle, trop grand = arrêt hors portée)
`UnreachableRetryDelay` : 3.0 s
`ApproachAcceptanceRadius` : 50 (sans rayon de capsule)

## Cas limites
- Cible morte mais non détruite : le pointeur faible reste valide, la vivacité est testée (existant) ; la réservation est libérée au rechoix.
- Zone enregistrée après le premier échec : la zone est relue à chaque choix, sans cache (existant, à ne pas casser).
- `MoveToLocation` rappelle `OnMoveCompleted` de façon synchrone (échec, déjà arrivé) : tout l'état (cible, réservation, exclusion) est écrit avant l'appel.
- Soldat posé à la main sans spline : comportement inchangé.
- Soldats entre eux (collision de capsules) : pas de crowd ni d'évitement ici ; la réservation réduit l'empilement sans l'annuler (cas « tous réservés »).

## Périmètre de fichiers
- Créer : `Source/Spooky_Spores/Enemies/EnemyTargetSubsystem.h/.cpp` ; `Source/Spooky_Spores/Tests/EnemyTargetReservationSpec.cpp`.
- Modifier : `Enemies/EnemyAIController.h/.cpp`, `Enemies/EnemySoldier.cpp` ; `Tests/EnemyTestActors.h` (contrôleur de test qui surcharge `RequestMoveToTarget`) ; `Tests/EnemyTargetingWorldSpec.cpp` (voir ci-dessous).
- `Spooky_Spores.Build.cs` : aucun changement attendu (`NavigationSystem` déjà présent). `DefaultEngine.ini` : aucun changement (agent `SoldierAgent` déjà défini ; à vérifier).
- Points chauds touchés plus tard : `Lvl_Principal.umap` (NavMeshBoundsVolume, voir intégration), `TODO.md` et `documentation-technique.md` (`consistency-keeper`, fin de branche : section 11 dit « `AddMovementInput`, pas `MoveToLocation` », à réécrire).

## Tests : Automation Spec contre PIE
- **Spec** : choix de cible, réservations et libérations, requête de déplacement (couture), échec sans NavMesh, logs d'erreur. Les tests observent `GetCurrentTarget()` et `GetClaimCount`, pas la position.
- **À réécrire** : les tests actuels de `EnemyTargetingWorldSpec` (C8, C9, cap en 2D, zone tardive) déduisent la cible du déplacement réel ; sans NavMesh ils échoueraient. Les passer sur `GetCurrentTarget()` / soldat posé à portée. Le test « 2D » garde son sens (cible 2D la plus proche, pas 3D).
- **Manuel (PIE)** : contournement réel d'un arbre ; 4 soldats se répartissent ; fin de vague non bloquée ; animation de marche (Anim Blueprint Debugger).

## Intégration éditeur (étapes humaines, après compilation et specs vertes)
- `NavMeshBoundsVolume` de `Lvl_Principal` : réduit à la base ; vérifier qu'il couvre la fin de la spline, le trajet jusqu'à la base et les destructibles (sinon l'agrandir : modification de `Lvl_Principal.umap`, à sérialiser).
- Vérifier que l'arbre et les destructibles affectent bien le NavMesh (`Can Ever Affect Navigation`) ; reconstruire le NavMesh.
- Vérifier quel agent de navigation la capsule de `BP_EnemySoldier` sélectionne (`SoldierAgent` rayon 34) et que `AutoPossessAI` reste `PlacedInWorldOrSpawned`.

## Hors périmètre
- Rôle attaque/défense des serviteurs (2.2c).
- Évitement entre soldats (crowd/RVO), `Transit` sur NavMesh, nouvelle logique d'attaque.
- Timeout de fin de vague (déjà refusé par Timéo).

## Décisions de Timéo
- Cible inatteignable : proposition de la spec (exclusion temporaire, autre cible, attente avec un seul log, pas de ligne droite) acceptée : « Je suis d'accord pour le point 1 / 2. »
- Soldats engagés : « Une fois que les soldats ont trouvé un point, il le continue jusqu'à en mourir ou jusqu'à ce qu'il soit détruit comme actuellement, mais on garde bien que au moment du choix, si il y a un autre objet destructible qui est libre, même si c'est pas le plus proche, il y va. »

## Règle transversale des tests
Chaque test de choix de cible doit échouer si le choix ignore les réservations (contrôle par mutation).
