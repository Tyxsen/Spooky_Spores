# Checklist d'intégration éditeur : servant-combat (2.2c)
Spec : `docs/specs/servant-combat.md`. Éditeur NON joignable à la rédaction (`NOT_CONNECTED`) : rien n'a été lu dans l'éditeur, tout ce qui suit est déduit du code et des docs. Les étapes 1 à 5 sont donc des lectures à confirmer.

**En tête : 5 étapes [MCP], toutes de lecture/vérification, aucune création.** Le C++ porte tout (composant de santé, défauts, contrôleurs). Rien d'autre à intégrer : pas de nouveau Blueprint, widget, asset, `.umap` ni `Default*.ini`. Si les défauts sont bons, `bp-integrator` n'a rien à écrire : l'humain peut faire les étapes 1 à 5 en 5 min dans l'éditeur.

## 1. Pré-requis
- Branche `feature/servant-combat` ; Automation Specs vertes (arrêt 2 de `CLAUDE.md`).
- Classes C++ visibles dans l'éditeur après rebuild : `AServant` (+ `HealthComponent`, `AttackDamage`, `AttackRange`, `AttackInterval`), `AEnemyAIController` (`RepathInterval`), `AServantAIController`, `AEnemySoldier`. `UEnemyRegistrySubsystem` : C++ seul, rien à voir dans l'éditeur.
- Assets existants (glob disque) : `/Game/Spooky_Spores/Placeables/Servants/BP_Servant`, `/Game/Spooky_Spores/Placeables/Enemies/BP_EnemySoldier`, `.../BP_EnemyWaveSpawner`.

## 2. Étapes
1. [MCP] `BP_Servant` : lister les composants. Attendu : exactement un `UHealthComponent`, nommé `HealthComponent` (hérité du C++, icône « inherited »). Un second (ajouté à la main dans le Blueprint, ex. `HealthComponent1`) = doublon : `GetHealthComponent` renverrait l'un, `ApplyDamage` pourrait toucher l'autre. Sa suppression est [HUMAIN] (étape 6).
2. [MCP] `BP_Servant`, Class Defaults, catégorie `Combat` : lire `AttackDamage` = 15, `AttackRange` = 100, `AttackInterval` = 1.2 ; fixer par `set_default` si différent. Attendu : relecture = ces trois valeurs.
3. [MCP] `BP_Servant`, composant `HealthComponent` : lire `MaxHealth` = 100 ; fixer si différent. Attendu : 100 (`CurrentHealth` est posé à `MaxHealth` au `BeginPlay`, ne pas y toucher).
4. [MCP] Serviteurs placés dans le niveau **ouvert** (`control_actor.find` par classe `BP_Servant`) : pour chacun, lire `AttackDamage`, `AttackRange`, `AttackInterval` et `MaxHealth`. Piège `CLAUDE.md` §8 : une valeur éditée à la main sur l'instance ignore les défauts. Attendu : 15 / 100 / 1.2 / 100 ; sinon rapporter la valeur lue, puis aligner l'instance (propriété d'acteur) si c'est voulu. Ne lit que le niveau ouvert : l'autre niveau (`Lvl_FirstPerson`) exige [HUMAIN] (ouverture de niveau).
5. [MCP] `BP_EnemySoldier` : lire la classe d'`AI Controller Class` (Class Defaults). Le C++ de `AEnemySoldier` ne la fixe pas (aucun `AIControllerClass` dans `Enemies/`), donc elle vient du Blueprint. Attendu : `EnemyAIController` (C++ direct). `RepathInterval` (1.0) vit sur le **contrôleur**, pas sur le soldat : il n'apparaît pas dans `BP_EnemySoldier` et ne peut être écrasé que par un Blueprint enfant du contrôleur. Si la classe lue est un Blueprint enfant (`BP_...AIController`), le dire : lire alors son `RepathInterval` (attendu 1.0). Si c'est le C++ direct : rien à faire, la valeur est celle du code. Lire aussi les soldats posés à la main, le cas échéant (même limite que 4).
6. [HUMAIN] Seulement si l'étape 1 révèle un doublon : supprimer le composant en trop dans `BP_Servant` (suppression), compiler/sauver le Blueprint.
7. [HUMAIN] Vérifier que le `NavMeshBoundsVolume` de `Lvl_Principal` couvre les zones où servants et soldats se rejoignent (touche `P` pour afficher le NavMesh). `documentation-technique.md` §12 : volume réduit à la base ; un `MoveToActor`/`MoveToLocation` hors volume échoue silencieusement.

## 3. Compilation et rechargement (humain, avant bp-integrator)
Fermer l'éditeur ; régénérer les fichiers de projet (clic droit sur le `.uproject`, nouveaux fichiers `EnemyRegistrySubsystem.*`) ; Rebuild complet dans Rider (pas Live Coding : `UPROPERTY`/`UFUNCTION` modifiées) ; lancer les Automation Specs (filtre `SpookySpores`) ; rouvrir l'éditeur sur la branche ; activer le serveur MCP natif (EngineWire) pour `bp-integrator`.

## 4. Scénario de test humain (PIE sur `Lvl_Principal`)
1. Poser 2 serviteurs (L), Tab : assigner A à attaque/défense, B laissé sans tâche. Fermer le menu.
2. Touche de test `StartWaves` : un soldat apparaît au point 0 de la spline et marche vers la base.
3. A quitte l'errance, va vers l'ennemi le plus proche de lui, le frappe (~1.2 s d'intervalle, PV du soldat en baisse), puis revient à l'errance quand plus d'ennemi. B ne combat jamais.
4. Soldats : ils ciblent aussi les serviteurs (A et B). Sur un serviteur en mouvement, re-visée environ toutes les secondes, sans zigzag excessif ni soldat figé.
5. Serviteur `FieldHarvest` frappé en pleine récolte : il continue ; s'il meurt, il disparaît sans os (aucun drop), son champ redevient libre, il n'est plus listé dans Tab (numéro non réutilisé).
6. Dé-assigner A pendant un combat : il cesse de frapper et retourne à l'errance.
7. Rayon d'acceptation `AttackRange * 0.5` : vérifier que le serviteur s'arrête à portée de coup sans pousser ni traverser l'ennemi, ni rester collé hors portée (coups réellement portés). Valider en jeu ; sinon rapporter le ressenti.
8. Mécaniques impactées (diff) : récolte puis replantage par un `FieldHarvest` ; errance (pauses, retour en zone) ; vague complète jusqu'à sa fin (chrono `WaveInterval` après la dernière mort) ; drops d'os à la mort d'un soldat (qui doivent toujours tomber) ; menu Tab (pause, assignation, fermeture).
