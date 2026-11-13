# CLAUDE.md — Spooky_Spores

Chargé à chaque session **et par chaque sous-agent** : uniquement les règles, conventions et pièges. L'état du projet vit ailleurs, ne le recopie pas ici.

| Besoin | Fichier |
|---|---|
| Présentation du jeu, 3C (déplacement, caméra, sorts, valeurs) | `README.md` |
| Intention de conception (boucle, monde, level design, DA) | `GDD.md` |
| Architecture par système et choix techniques | `documentation-technique.md` |
| File d'attente, dette technique, limites connues | `TODO.md` |
| Workflow d'agents (rôles, pipeline, branches, garde-fous) | `docs/WORKFLOW_AGENTS.md` |

---

## 1. Méthode de travail

- **Par défaut : pipeline d'agents** (section 10). Le code C++ est écrit par `cpp-implementer` d'une spec que Timéo a validée. Mode manuel (Claude guide, Timéo écrit) seulement si Timéo le demande pour une tâche donnée.
- **Avant toute action** (modification, commande, génération) : explique ce que tu veux faire et comment. Commandes Git ou shell risquées (reset, suppression, force push) : explique d'abord les conséquences.
- `CLAUDE.md` : propose la modification, applique-la après validation de Timéo. README, doc technique, TODO : modifiables sur demande.
- **Avant le test en éditeur** (Blueprints, assets, PIE) : revue du code C++ écrit entretemps, sans attendre qu'on te la demande.
- **Questions à Timéo** : toujours sous la forme « Problème → ma proposition → qu'en penses-tu ? ». Seulement les décisions de gameplay ou de direction ; les choix purement techniques se tranchent et se notent.
- Réponds **en français**, de façon didactique (le « pourquoi »), directe et concise, sans flatterie.
- **Challenge Timéo** : ne valide jamais une idée par défaut. Faille, meilleur chemin, risque de scope, contradiction avec une décision passée : dis-le et argumente. Quand son argument est bon, reconnais-le ; quand tu te trompes, dis-le.
- **Exactitude** : ne donne que des informations vérifiées, sinon dis que tu ne sais pas. Projet sur **Unreal Engine 5.8.3** : pour toute API susceptible d'avoir changé, vérifie la doc officielle ou le code moteur local plutôt que de répondre de mémoire.
- **Principes de code** : KISS, DRY, SOLID appliqués concrètement. Systèmes génériques et modulaires, pas d'abstraction ni d'optimisation prématurée. Pas de durcissement défensif (NaN, infini) hors frontière de confiance (données externes, sauvegarde).
- **Tests** : ils vérifient le comportement réel, cas limites compris, et doivent pouvoir échouer (contrôle par mutation : casser le code, le test passe au rouge). « Ça compile » ne veut pas dire « ça marche ». Avant chaque fusion : Automation Specs vertes, et Timéo teste les mécaniques que le code modifié a pu impacter.
- **Cahier des charges** : qualité technique avant quantité ; bac à sable systémique ; architecture modulaire, systèmes indépendants du Level Blueprint. L'IA est autorisée comme assistance.

---

## 2. Environnement

- Unreal Engine **5.8.3** (le `.uproject` référence `"5.8"`). **Les deux ordinateurs doivent être sur le même hotfix.** IDE : Rider. Git LFS installé sur chaque machine (`git lfs install` avant le clone).
- Logique et systèmes en C++ ; Blueprints pour configurer et polir. Module `Spooky_Spores`, macro `SPOOKY_SPORES_API`. Base : template First Person. Carte par défaut : `Lvl_Principal` ; `Lvl_FirstPerson` reste un niveau de test.
- **Code** (`Source/Spooky_Spores/`) : un dossier par domaine fonctionnel (liste dans `documentation-technique.md` section 2, sinon sur le disque). Un nouveau domaine = un nouveau dossier ; sous-dossiers seulement au-delà de 6-8 fichiers. Fichiers du module et `CollisionChannels.h` à la racine. `Tests/` : Automation Specs, un fichier par système (Rider : clic droit sur le dossier > *Run Unit Tests*, ou panneau Automation, filtre `SpookySpores`).
- **Content** (`Content/Spooky_Spores/`) : inspiré du style guide d'Allar. `Core/` (ce qu'on ne modifie pas à la légère), `Placeables/`, `UI/Widgets/`, `Maps/`.
- **`__ExternalActors__` / `__ExternalObjects__` ne sont pas inutilisés** : `Lvl_FirstPerson` utilise One File Per Actor. `Lvl_Principal` non : ses acteurs sont dans `Lvl_Principal.umap` (point chaud, jamais modifié sur deux branches en parallèle).

---

## 3. Conventions de code

- **Includes** : chemin depuis la racine du module (`#include "Interaction/Grabbable.h"`), jamais d'include court. Dans un header : forward declaration pour un simple pointeur ; includes complets dans le `.cpp`, explicites (`Engine/World.h`, `TimerManager.h`…), sans inclusion indirecte.
- **Accès** : le plus restrictif (`private` > `protected` > `public`). Callbacks `AddDynamic` : `UFUNCTION()` privées. Les `_Implementation` d'interface sont publiques.
- **Header** : deux groupes `// -----------FUNCTIONS-----------` et `// -----------PROPERTIES-----------` (11 `-` de chaque côté), chacun avec `public`, `protected`, `private`. Référence : `ResourceDrop.h`. Pour tout nouveau fichier ; pas de reprise des anciens.
- **UPROPERTY** : composant créé au constructeur `VisibleAnywhere, BlueprintReadOnly` ; réglage `EditAnywhere` + `Category = "Domaine|Sous-partie"` ; membre `private` exposé : `meta = (AllowPrivateAccess = "true")` ; valeur runtime à inspecter : `VisibleInstanceOnly`. **Toujours une valeur par défaut explicite** (enums compris).
- **`TObjectPtr<T>`** pour les membres `UPROPERTY` (en place dans `Resources/`, adoption par domaine). `GrabComponent` et `Spooky_SporesCharacter` restent en pointeurs bruts. Ne jamais mélanger les deux styles dans un fichier.
- **Interfaces** : dans l'interface, uniquement `UFUNCTION(BlueprintNativeEvent, BlueprintCallable, ...)`, **sans** déclarer `_Implementation` (UE 5.8 la génère ; la redéfinir donne C2084) ; les `.cpp` d'interface restent vides. Classe concrète : `virtual T F_Implementation(...) override;` (signature identique, `const` compris). Test : `Actor->Implements<UMonInterface>()` (version **U**). Appel externe : `IMonInterface::Execute_F(Cible, ...)`, jamais `_Implementation`.
- **Nommage** : commentaires de code en **anglais** (discussion et docs en français). Préfixes `BP_`, `WBP_`, `GC_`, `IA_`. Fichiers C++ sans préfixe de classe. Booléens en question (`IsHolding`). **Jamais un paramètre ou local du nom d'un membre existant** (C4458 ; `AActor` a `Instigator`, d'où `InteractingActor`).
- **Divers** : racine d'un acteur via `SetRootComponent(...)`, jamais un `RootComponent` redéclaré. Les nœuds latents (`Delay`) sont interdits dans une **fonction** Blueprint (Custom Event dans l'Event Graph).

---

## 4. Architecture : principes

- Schéma du CDC : **Joueur → Interaction → Objets interactifs → Systèmes physiques → Environnement**. Aucun système ne connaît l'implémentation d'un autre.
- Deux interfaces séparées : `IGrabbable` (manipulable physiquement, `GetGrabbableComponent()`) et `IInteractable` (comportement à déclencher, `OnInteract(AActor* InteractingActor)`). Un objet implémente l'une, l'autre ou les deux.
- `UInteractionComponent` fait **uniquement de la détection** (un trace par frame) ; `UGrabComponent` uniquement de la télékinésie. **Le Character arbitre** (touche E contextuelle) : aucun composant n'en appelle un autre.
- Tous les traces s'appuient sur l'override de `GetActorEyesViewPoint()` du Character (la caméra réelle, pas le défaut du moteur).
- Les widgets **reçoivent** leurs dépendances, ils ne les cherchent pas.
- Types de ressources : `EResourceType` (`Resources/ResourceTypes.h`). Ajouter un type = une valeur + une entrée dans `WBP_ResourceCounter`.

## 5. Collisions

- Canaux custom : `Interactable` (Object Channel, Block par défaut), `ResourceDrop` (Object Channel, Block par défaut, pour que les drops s'ignorent entre eux), `Projectile` (template). `Interactable` est un **Object Channel** : trace via `LineTraceSingleByObjectType` + `FCollisionObjectQueryParams`. Un canal custom occupe un slot `ECC_GameTraceChannelN` (`Config/DefaultEngine.ini`) ; alias dans `CollisionChannels.h` seulement pour un canal utilisé en code.
- La **réponse par défaut** d'un canal décrit comment **les autres presets** réagissent à lui (mettre `Ignore` sur `ResourceDrop` ferait tomber les drops à travers le sol).
- Presets : `PhysicsInteractable` (Query and Physics, Object Type Interactable, Block partout ; **requis pour qu'un acteur soit vu par le trace**), `ResourceDrop`, `Trigger` (intégré, `AStoryTrigger`).
- `Query` = traces et overlaps, `Physics` = simulation. Entre deux objets, la réponse la plus faible l'emporte. Overlap et hit exigent aussi `SetGenerateOverlapEvents(true)` / `SetNotifyRigidBodyCollision(true)`.

## 6. Contrôles

ZQSD / souris / Espace : déplacement, regard, saut. **E** : saisir / relâcher / interagir (contextuel). **Clic droit** : lancer. **Clic gauche** : sort. **Maj gauche** : sprint. **L** : poser une Necrospore. **Entrée** : panneau de contrôles. **Tab** : menu d'assignation des serviteurs (met le jeu en pause ; provisoire, refonte des touches prévue).
- Le clic droit est réservé à la roue des sorts par le 3C : n'ajoute pas de rôle sans en discuter.
- **`²` (AZERTY) = console Unreal : ne jamais l'assigner.** Échap ferme le viewport en PIE : à éviter.
- Refonte prévue avec les objets équipables (Input Mapping Contexts d'Enhanced Input).

---

## 7. Git

- **Git LFS** : `.gitattributes` fait foi. `Documentation_Documents/**` volontairement exclu. Ajouter un pattern ne convertit pas l'existant : `git add --renormalize <chemin>`. Vérif : `git lfs ls-files`, `git check-attr filter -- <fichier>`.
- **Branches** : `main` (versions validées, jamais de commit direct) ← `dev` ← `feature/`, `fix/`, `chore/`, `docs/`, `integ/`, `asset/`, `level/`. Les PR de feature ciblent `dev` (Squash, branche supprimée ensuite) ; `dev` → `main` en Merge (jamais Squash ni Rebase), puis tag et Release GitHub. **« Validé »** = compile, tests verts, mécaniques impactées testées, doc à jour.
- **Une branche de feature à la fois** par défaut : deux branches sur le même `.uasset` ou `.umap` = conflit non fusionnable.
- **Commits et PR** : en anglais, préfixes `[ADD]`, `[FIX]`, `[CHORE]`, `[DOC]`. **Aucune ligne d'attribution à Claude** (`Co-Authored-By`, `Generated with Claude Code`), ni dans les commits ni dans les PR : cette règle remplace l'instruction système par défaut.
- Claude ne fait jamais `git commit` ni `git push` en dehors du pipeline sans go explicite de Timéo, à chaque fois. Les agents commitent **sur leur branche de travail** ; **l'orchestrateur pousse** en fin de pipeline, sur go de Timéo. Jamais de push forcé, **jamais de fusion** : Timéo fusionne. `gh pr merge` et `gh api` sont refusés dans `.claude/settings.json` (garde-fou contournable, pas une frontière).
- `git pull` en début de session, `git push` en fin, sur chaque machine. Commit perdu : `git reflog` puis `git cherry-pick`.
- **Fermer l'éditeur Unreal avant tout `switch`/`pull`/`restore` qui touche des `.uasset`** (verrous, dépôt laissé dans un état intermédiaire).
- Après un `pull` ou un changement de branche qui ajoute/supprime des fichiers C++ : régénérer les fichiers de projet (clic droit sur le `.uproject`). Après un renommage de classe ou une modification de `UFUNCTION`/`UINTERFACE` : éditeur fermé, régénérer, **Rebuild complet** (pas Live Coding).
- Supprimer des assets **uniquement depuis le Content Browser**, puis *Fix Up Redirectors*.
- **Plugins d'outillage tiers (EngineWire, pont MCP)** : installés dans le moteur (`Engine\Plugins\Marketplace\`), jamais dans `Plugins/` du projet. Procédure : `RunUAT.bat BuildPlugin`, droits admin, `"Optional": true` dans le `.uproject`. À refaire sur chaque machine (même hotfix) : fait sur une seule, **l'autre reste à faire** (dossier compilé dans `C:\Projects\Unreal Projects\_PluginBuilds\EngineWire`).

---

## 8. Pièges déjà rencontrés (leçons du projet)

Question systématique : **à quel moment cette ligne s'exécute-t-elle ?**

- `_Implementation` redéclaré ou défini dans une interface → C2084 (section 3). Membre `RootComponent` redéclaré → l'acteur n'a pas de racine. Paramètre nommé `Instigator` → C4458. Variable locale portant le nom d'un `UPROPERTY` → la valeur de l'éditeur est ignorée.
- `FindOrAdd` récupéré par copie → le compteur ne s'incrémentait jamais. `FindComponentByClass` sans cible → cherche sur `this`. Ajouter un `float` à un `FVector` → ajouté aux trois composantes.
- `Destroy()` dans `BeginPlay` → chaque drop se détruisait à sa création. `return` au lieu de `continue` dans une boucle de spawn → fin de fonction sautée.
- `SpawnActor` exécute `BeginPlay` et les overlaps **avant** de rendre la main → utiliser le spawn différé (`SpawnActorDeferred` → initialisation → `FinishSpawning`). Collision désactivée au constructeur au lieu du moment de l'interaction → nœud invisible pour le trace.
- `FHitResult.Location` utilisé quand le trace n'a rien touché → vaut `(0,0,0)`. `DrawDebugLine` persistante appelée à chaque tick → accumulation. `GetActorEyesViewPoint` par défaut ≠ caméra réelle → trace décalé.
- Drops apparus les uns dans les autres, qui se bloquaient → vitesses d'expulsion énormes (résolu par le canal `ResourceDrop` en Ignore). Damage Threshold par défaut trop élevé pour un petit objet.
- `Delay` dans une fonction Blueprint → interdit (Custom Event). Nœud `==` numérique au lieu de `Equal (Enum)` → impossible de brancher un enum.
- Touche assignée qui semblait libre mais ouvrait la console (`²` en AZERTY) → vérifier les bindings réservés avant d'assigner.
- Une propriété modifiée à la main sur une **instance placée dans le niveau** (ex. `AttackRange`, points d'une spline) ignore ensuite toute nouvelle valeur par défaut du C++ ou du Blueprint, tant qu'elle n'est pas réinitialisée (`Reset to Default`) : vérifier l'instance placée, pas seulement la classe.
- `UPathFollowingComponent::HasReachedInternal` (utilisé par `AIController::MoveToLocation`) mesure la distance d'arrivée **en 2D uniquement** : une distance 3D calculée à la main ne concordera jamais avec le `AlreadyAtGoal` du moteur.
- `bStopOnOverlap` de `MoveToLocation` ajoute **automatiquement** le rayon de capsule de l'agent (arrêt et test d'arrivée) : l'ajouter à la main dans `AcceptanceRadius` le compte en double.
- `Default Query Extent` (`Project Settings > Navigation System`) trop petit empêche de projeter un point situé dans la marge d'exclusion du NavMesh autour d'un obstacle : `MoveToLocation` échoue silencieusement (`Failed`) même avec `bProjectDestinationToNavigation = true`.
- Un personnage déplacé via `MoveToLocation`/`PathFollowingComponent` impose une **vitesse** (`RequestDirectMove`) et non une accélération, sauf si `GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true` (défaut `false`). Un Anim Blueprint qui lit `GetCurrentAcceleration` (ex. `ABP_Unarmed`, `Should Move`) fait alors glisser le personnage sans cycle de marche. Corrigé et confirmé sur le serviteur ; vérifier avec l'Anim Blueprint Debugger.
- `TActorIterator<T>` ne garantit aucun ordre : ne jamais s'en servir pour choisir « le bon » acteur parmi plusieurs du même type ; préférer une référence explicite assignée à la main (`EditInstanceOnly`). Si le consommateur est créé par le code (serviteur spawné) ou par un `PlayerStart` (Character), aucune instance du niveau ne peut porter cette référence : passer par un `UWorldSubsystem` qui enregistre l'unique instance (modèle : `UBaseZoneSubsystem`, la première enregistrée gagne, un log d'erreur signale la seconde).
- `UFUNCTION(BlueprintCallable)` `const` avec un paramètre de sortie devient **pure** automatiquement (UHT) : un nœud pur est réévalué à chaque pin de sortie, donc un tirage aléatoire donnerait deux valeurs. Ajouter `BlueprintPure = false`.
- Un menu qui met le jeu en pause ne doit pas utiliser `FInputModeUIOnly` : il appelle `SetIgnoreInput(true)` et la touche de fermeture n'arrive plus. Utiliser `FInputModeGameAndUI` et une Input Action avec `Trigger when Paused`.
- Dans un Blueprint, enchaîner une étape après un `For Each Loop` via son `Loop Body` au lieu de `Completed` (ou d'un `Sequence`) la saute quand la liste est vide et la répète par élément sinon.
- `MoveToLocation` (échec, `AlreadyAtGoal`) et `StopMovement` (`Aborted`) appellent `OnMoveCompleted` de façon synchrone, avant que la fonction n'ait rendu la main : tout état que ce callback lit (`bIsWandering`, réservation, cible) doit être remis à zéro avant l'appel, sinon il replanifie ou résout un déplacement déjà annulé.
- Un acteur qui réagit à un delegate dont il est lui-même la cause (ex. le serviteur qui récolte un champ et reçoit `OnFieldChanged`) doit ignorer ses propres changements pendant l'action (garde `bIsResolving` / `TGuardValue`), sinon il s'annule lui-même.

---

## 9. Planning

- Pas de découpage par semaines : on déroule `TODO.md`. Timéo signale lui-même chaque deadline ; ne suppose pas d'échéance non annoncée.
- **Une mécanique par tâche, livrée entière** avant la suivante. Quand une tâche déborde, on coupe ou on repousse en fin de file. IA (ennemi, serviteur) et conquête du village : minimum viable, premiers candidats à la coupe.
- Objectif de fond : la **boucle principale** jouable de bout en bout (récolter → planter → potion → combattre → os → serviteur → le serviteur récolte).
- **Avant toute Release** : retirer le code et les nœuds de debug et rétablir les réglages de développement (ex. `Hidden In Game`), listés dans la dette technique de `TODO.md`.

---

## 10. Workflow d'agents

Détail : `docs/WORKFLOW_AGENTS.md`. Agents dans `.claude/agents/`. La session principale est l'**orchestrateur** : elle délègue, vérifie, ne code pas à la place des agents.

| Tâche | Agents |
|---|---|
| Nouvelle mécanique | `game-designer` → spec → `cpp-implementer` → `qa-test-writer` → `code-reviewer` + `consistency-keeper` (rapport) + `bp-integration-planner` → compilation + tests → `bp-integrator` → `editor-verifier` → test humain → `consistency-keeper` (docs) |
| Bug C++ | `cpp-implementer`, `qa-test-writer`, `code-reviewer` |
| Câblage d'un asset existant | `bp-integration-planner` → `bp-integrator` → `editor-verifier` |
| Valeur d'équilibrage, réécriture de docs | `consistency-keeper` seul |

**Arrêts obligatoires** (même sans demande) : 1. après la spec ; 2. avant `bp-integrator` (Timéo compile **et lance les Automation Specs**) ; 3. après `editor-verifier` (test PIE de la feature et des mécaniques impactées, par Timéo) ; 4. avant le push (l'orchestrateur pousse la branche sur go de Timéo ; Timéo ouvre la PR et la fusionne, l'orchestrateur ne le fait jamais). « Fais la tâche X » = jusqu'au prochain arrêt.

**Règles**
- Une unité de travail = une branche depuis `dev`. Avant d'accepter le travail d'un agent, comparer `git diff --name-only` au « Périmètre de fichiers » de la spec. Avant deux branches parallèles, vérifier que leurs périmètres ne se recoupent pas.
- **Points chauds, toujours sérialisés** : `EResourceType`, `*.Build.cs`, `Default*.ini`, `TODO.md`, `CLAUDE.md`, `documentation-technique.md`, `Lvl_Principal.umap`. Seul `consistency-keeper` met à jour les docs partagés, en fin de branche, et ne modifie jamais `CLAUDE.md`.
- **Briefs aux agents** : citer les décisions de Timéo **mot pour mot**, sans reformuler. Spec : ≤ 100 lignes, sinon découper la feature.
- **Spec et checklist d'intégration sont temporaires** : l'orchestrateur les supprime dans le dernier commit de la branche, avant la PR. Reste durable : le code, les tests, la doc technique.
- Seul `bp-integrator` écrit dans l'éditeur (MCP Unreal), un seul à la fois, après compilation. L'orchestrateur vérifie les préconditions (branche, arbre propre, éditeur joignable via `ue_project_info`) **avant** de le lancer.
- Un agent qui doute le dit au lieu d'inventer ; il ne délègue à aucun autre agent.
