# Workflow d'agents — Spooky_Spores

Ce document décrit comment un orchestrateur (la session Claude Code principale) délègue le travail à des sous-agents spécialisés, comment ce travail est isolé dans des branches git, et à quels moments l'humain valide.

**Statut** : document de conception du workflow. Les 8 agents du pipeline principal sont créés dans `.claude/agents/` (2026-11-10), **à relire et jamais testés** : `game-designer`, `cpp-implementer`, `qa-test-writer`, `code-reviewer`, `consistency-keeper`, `bp-integration-planner`, `bp-integrator`, `editor-verifier`. `asset-creator`, `level-designer` et `art-audio-checker` ne sont pas créés.

**Moteur** : Unreal Engine **5.8.3** (confirmé le 2026-11-10 dans `Engine/Build/Build.version` de l'installation locale).

**Sources canoniques** : le jeu est décrit dans [`GDD.md`](../GDD.md), le code dans [`documentation-technique.md`](../documentation-technique.md), le backlog dans [`TODO.md`](../TODO.md). Ce document ne les duplique pas : il décrit uniquement la façon de travailler.

---

## 1. Principes

1. **Un agent = un rôle, une sortie, des droits limités.** Un agent ne fait que ce que son rôle prévoit, et n'a que les outils dont il a besoin.
2. **Une unité de travail = une branche.** Chaque tâche confiée à un agent d'écriture vit dans sa propre branche (voir section 5). C'est ce qui permet à plusieurs agents de travailler en même temps sans se gêner.
3. **La spec est le contrat.** Un sous-agent ne voit pas la conversation : il ne connaît que ce qu'on lui transmet. Toute mécanique passe donc par une spec écrite (`docs/specs/<item>.md`), qui sert de pivot entre designer, implémenteur et testeur.
4. **Un seul écrivain par fichier à un instant donné.** Les fichiers binaires (`.uasset`, `.umap`, `.blend`) ne sont pas fusionnables : deux branches ouvertes ne doivent jamais modifier le même fichier (voir section 6).
5. **L'humain reste l'arbitre de la conception.** Le GDD est sa décision. Les agents proposent, l'humain valide les points de contrôle.
6. **Contrôles indépendants.** Les agents qui vérifient (tests, revue, cohérence) n'ont pas écrit ce qu'ils vérifient, et sont en lecture seule.

### Frontière agents / éditeur Unreal

- **Les agents produisent** : le C++, les tests, les specs, la documentation, les briefs et checklists, et, par `bp-integrator` uniquement, l'intégration dans l'éditeur Unreal via le MCP Unreal (Blueprints enfants, composants, valeurs par défaut, placement), strictement limitée aux étapes [MCP] d'une checklist.
- **L'humain fait** : la validation de la spec, la compilation (fermer l'éditeur, rebuild dans Rider, rouvrir sur la branche), ce que les outils ne couvrent pas (Event Graph, mise en page des widgets UMG, réglage fin d'éclairage et de level design), le **test de la feature en éditeur (PIE)** et la décision de fusionner. Les fusions vers `dev` sont toujours les siennes.
- **Exception encadrée** : `bp-integrator` est le seul agent autorisé à écrire des `.uasset` et à placer des acteurs, avec une liste d'outils restreinte (voir section 10). Aucun autre agent ne modifie un `.uasset` ou un `.umap`.

Pour que le travail dans l'éditeur se réduise à du câblage, l'implémenteur C++ expose une API propre : `UFUNCTION(BlueprintCallable)`, `UPROPERTY(EditAnywhere)`, événements `BlueprintImplementableEvent`.

---

## 2. Les rôles

| Rôle | Mission | Produit | Droits d'écriture |
|---|---|---|---|
| **game-designer** | Transforme un item de `TODO.md` en spec précise, fidèle au GDD | `docs/specs/<item>.md` | `docs/specs/` uniquement |
| **cpp-implementer** | Implémente la spec en C++ avec une API exposée aux Blueprints, en respectant les conventions de `CLAUDE.md` section 3 (recopiées dans son prompt) | Code dans `Source/` | `Source/` uniquement |
| **qa-test-writer** | Écrit les tests **à partir de la spec seule**, sans lire l'implémentation | Tests Automation Spec (ou Functional) | Dossiers de tests uniquement |
| **asset-creator** | Crée ou modifie un modèle 3D source via le MCP Blender, selon un brief | `.blend`, export `.fbx`, textures | `Art/Source/<asset>/` uniquement |
| **bp-integration-planner** | Prépare la checklist d'intégration éditeur (Blueprints, import d'assets) : étapes [MCP] pour `bp-integrator`, étapes [HUMAIN] pour le reste, scénario de test | Checklist `docs/integration/<item>.md` | `docs/integration/` uniquement ; lecture seule dans l'éditeur |
| **bp-integrator** | Exécute dans l'éditeur, via le MCP Unreal, les étapes [MCP] de la checklist, à la place de l'humain. Vérifie des préconditions (branche, projet, arbre propre, C++ compilé) avant de commencer, refuse tout hors checklist | Assets et acteurs dans l'éditeur, commit, rapport `docs/integration/<item>-report.md` | Éditeur Unreal (outils MCP restreints), stage uniquement les fichiers de la checklist, rapport |
| **editor-verifier** | Contrôle indépendant : relit l'état réel de l'éditeur et le compare à la checklist, repère les effets de bord | Rapport (OK / KO / non vérifié par étape) | Aucun (lecture seule, dans l'éditeur comme dans le dépôt) |
| **level-designer** | Produit un brief de placement vérifié contre les piliers du GDD (section 3) | `docs/level/<zone>.md` | `docs/level/` uniquement |
| **code-reviewer** | Relit le diff : bugs, conventions UE (UPROPERTY, GC, réplication), dette | Rapport de revue | Aucun (lecture seule) |
| **consistency-keeper** | Détecte les contradictions entre GDD, `TODO.md`, doc technique et code ; propose les corrections de docs | Rapport + propositions | Aucun par défaut ; docs seulement sur ordre de l'orchestrateur |
| **art-audio-checker** *(plus tard)* | Valide un asset ou une ambiance contre la section 5 du GDD | Rapport | Aucun (lecture seule) |

### Pourquoi le testeur ne voit pas l'implémentation

Un agent qui écrit le code puis ses propres tests écrit presque toujours des tests qui valident ce qu'il vient de faire, pas ce qui était voulu. Le `qa-test-writer` part de la spec et du GDD : il teste le comportement attendu. C'est la traduction directe de la règle « pas de tests de complaisance ». Les tests doivent par ailleurs être rigoureux par rapport à l'architecture du projet : pas de test pour le plaisir d'avoir un test.

---

## 3. Table de routage

L'orchestrateur ne mobilise que les agents utiles à la tâche.

| Type de tâche | Agents mobilisés |
|---|---|
| Nouvelle mécanique | game-designer → (validation spec) → cpp-implementer → qa-test-writer → code-reviewer + consistency-keeper (rapport) + bp-integration-planner → (compilation par l'humain) → bp-integrator → editor-verifier → (test humain) → consistency-keeper (mise à jour des docs) |
| Correction de bug C++ | cpp-implementer, qa-test-writer (test de non-régression), code-reviewer |
| Nouvel asset 3D | asset-creator → bp-integration-planner (checklist d'import) → (humain : import et vérification) |
| Intégration d'un asset existant | bp-integration-planner → bp-integrator → editor-verifier |
| Level design d'une zone | level-designer, éventuellement art-audio-checker |
| Câblage d'un Blueprint | bp-integration-planner → bp-integrator → editor-verifier (les étapes [HUMAIN] restent à l'humain) |
| Changement de valeur d'équilibrage | consistency-keeper uniquement (propage dans GDD, README, doc technique) |
| Réécriture de `TODO.md` ou de docs | consistency-keeper uniquement |

Cette table figure aussi dans `CLAUDE.md` section 10, avec les arrêts obligatoires. Une délégation automatique par description existe dans Claude Code, mais la table explicite est préférée : plus prévisible.

---

## 4. Le pipeline d'une mécanique

```
TODO.md (item) ──► game-designer ──┬─ décision de gameplay ouverte : note de cadrage
                                   │   (problème → proposition) ─► l'HUMAIN tranche ─┐
                                   └─ aucune décision ouverte ───────────────────────┤
                                                                                     ▼
                                              docs/specs/<item>.md (100 lignes max)
                                          │
              ╔═══════════════════════════╧══════════════════════════╗
              ║ HUMAIN n°1 : relit la spec (OK / corrections)        ║
              ╚═══════════════════════════╤══════════════════════════╝
                                          ▼
                                   cpp-implementer
                                          ▼
                   qa-test-writer (depuis la spec seule)
                                          │
                ┌─────────────────────────┼─────────────────────────┐
                ▼                         ▼                         ▼
          code-reviewer         consistency-keeper        bp-integration-planner
          (rapport)             (rapport, lecture seule)  (checklist [MCP]/[HUMAIN])
                └─────────────────────────┼─────────────────────────┘
                     revue bloquante ? ───┴─── oui : retour au cpp-implementer
                                          ▼
              ╔══════════════════════════════════════════════════════╗
              ║ HUMAIN n°2 (facultatif) : parcourt la checklist      ║
              ║ HUMAIN n°3 (obligatoire) : compile (ferme l'éditeur, ║
              ║   rebuild dans Rider, rouvre sur la branche), puis   ║
              ║   lance les Automation Specs (clic droit sur Tests/  ║
              ║   > Run Unit Tests) : tout vert avant de continuer   ║
              ╚═══════════════════════════╤══════════════════════════╝
                                          ▼
                       bp-integrator (préconditions, puis étapes [MCP],
                                      commit, rapport)
                                          ▼
                       editor-verifier (lecture seule, OK/KO/non vérifié)
                                          ▼
              ╔══════════════════════════════════════════════════════╗
              ║ HUMAIN n°4 : teste en éditeur (PIE) + non-régression ║
              ╚═════════════╤════════════════════════════╤═══════════╝
                   non : retours (cpp-implementer       oui
                   ou bp-integrator)                      ▼
                                          consistency-keeper (met à jour les docs)
                                                          ▼
                                    PR vers dev : l'HUMAIN fusionne (Squash)
```

La note de cadrage n'est pas un arrêt de plus : c'est un point de décision à l'intérieur de l'arrêt n°1. Seules les décisions de gameplay ou de direction sont posées à l'humain ; les choix purement techniques sont tranchés par l'agent et notés en une ligne dans la spec.

Le parallélisme existe seulement là où il est sûr : les trois contrôles finaux (lecture seule ou fichiers distincts), et des unités de travail indépendantes sur des branches séparées (section 5).

---

## 5. Modèle de branches : une unité de travail, une branche

### Hiérarchie

`main` ← `dev` ← branches de travail. Aucun agent ne commite directement sur `dev` ou `main`. `main` reste à l'humain seul.

### Préfixes de branches

| Préfixe | Contenu | Agent écrivain | Exemple |
|---|---|---|---|
| `feature/` | Code C++ d'une mécanique, ses tests **et son intégration éditeur** (une seule branche, du C++ jusqu'au test humain) | cpp-implementer, qa-test-writer, bp-integrator | `feature/bone-resource` |
| `asset/` | Fichiers sources d'un asset (Blender, FBX, textures) | asset-creator | `asset/servant-skeleton` |
| `integ/` | Intégration éditeur **sans nouveau C++** (câblage ou import d'un asset existant) | bp-integrator (l'humain pour les étapes [HUMAIN]) | `integ/servant-skeleton` |
| `level/` | Briefs et ajustements de level design | level-designer (briefs), humain (éditeur) | `level/base-zone` |
| `fix/` | Correction de bug | cpp-implementer | `fix/cauldron-recipe-null` |
| `docs/` | Documentation, specs, synchronisation | consistency-keeper, game-designer | `docs/sync-hp-values` |

### Pourquoi séparer `asset/` et `integ/`

Créer un modèle dans Blender et l'intégrer dans Unreal sont deux travaux différents, qui n'utilisent ni les mêmes outils ni les mêmes fichiers. Les mettre sur deux branches permet :
- qu'un agent crée l'asset pendant qu'un autre agent travaille sur du C++ sans aucune interférence ;
- de relire et valider l'asset avant qu'il ne touche le moteur ;
- de rejeter ou refaire l'asset sans polluer l'historique de l'intégration.

### Dépendance entre branches : deux stratégies

Quand `integ/x` dépend de `asset/x`, il y a deux façons de procéder :

1. **Fusion d'abord (recommandée).** `asset/x` est fusionnée dans `dev` par une petite PR validée, puis `integ/x` est créée depuis `dev`. Historique propre, aucun risque de branche orpheline.
2. **Branches empilées.** `integ/x` est créée depuis `asset/x` sans attendre la fusion, ce qui permet de ne pas bloquer. Règle de sécurité : **profondeur maximale de 2**, et si `asset/x` est modifiée après coup, `integ/x` doit être rebasée.

L'orchestrateur choisit la stratégie 1 par défaut, et la stratégie 2 uniquement si l'humain veut avancer sans attendre.

### Règles de nommage et de commit

- Nom : `<préfixe>/<item-en-kebab-case>`, relié à un item de `TODO.md` ou à une spec.
- Message de commit : format du projet (`CLAUDE.md` section 7), **en anglais** : préfixe `[ADD]`, `[FIX]`, `[CHORE]` ou `[DOC]` suivi d'un résumé, par exemple `[ADD] Bone resource type`. **Aucune ligne d'attribution** (`Co-Authored-By`, mention « Generated with Claude Code ») dans les commits ni dans les descriptions de PR, y compris celles des sous-agents.
- Un commit = une intention. Pas de commit « divers ».

### Cycle de vie d'une branche

1. L'orchestrateur crée la branche depuis `dev` (jamais depuis une autre branche de travail, sauf stratégie 2).
2. Les agents commitent uniquement dans cette branche, et uniquement dans leur périmètre de fichiers.
3. Les contrôles (revue, cohérence, tests) tournent sur la branche.
4. L'humain valide et ouvre la PR vers `dev`. Les fusions vers `dev` sont toujours validées par l'humain, et se font en **Squash** (une branche = un commit sur `dev`, comme dans `CLAUDE.md` section 7) : l'historique intermédiaire des agents n'est pas conservé sur `dev`.
5. La branche est supprimée après fusion.

---

## 6. Éviter les conflits : périmètre de fichiers

### Le manifeste de périmètre

Chaque spec contient une section **« Périmètre de fichiers »** qui liste les fichiers et dossiers que la branche est autorisée à modifier. Avant de lancer deux branches en parallèle, l'orchestrateur vérifie que leurs périmètres **ne se recoupent pas**. S'ils se recoupent, les branches sont traitées l'une après l'autre.

### Fichiers « points chauds » : toujours sérialisés

Certains fichiers sont touchés par presque toutes les mécaniques. Ils ne sont jamais modifiés en parallèle :
- `EResourceType` (enum partagée) et les tables de données associées ;
- les fichiers `*.Build.cs` et les `Default*.ini` ;
- `TODO.md`, `CLAUDE.md`, `documentation-technique.md` ;
- tout `.umap` de niveau principal.

Pour la documentation, la règle est simple : **seul le consistency-keeper met à jour les documents partagés, en fin de branche**, pas les agents d'écriture pendant leur travail. Cela évite que dix branches modifient `documentation-technique.md` en même temps.

### Fichiers binaires et Git LFS

Les `.uasset`, `.umap`, `.blend`, `.fbx` et textures ne sont pas fusionnables. Conséquences :
- **Git LFS est en place** (vérifié le 2026-11-10). Patterns suivis : `*.uasset`, `*.umap`, `*.png`, `*.jpg`, `*.wav`, `*.fbx`, `*.psd` ; `Documentation_Documents/**` est volontairement exclu. **Non suivis à ce jour** : `.blend` et `.tga` (14 fichiers `.TGA` du pack `RockEnv_Pack`, environ 50 Mo, sont déjà commités hors LFS), et `.blend1` (sauvegarde Blender) n'est pas ignoré par `.gitignore`. Ces correctifs sont prévus sur une branche `chore/` séparée, pas dans ce document. Ajouter un pattern ne convertit pas les fichiers déjà commités (voir `git add --renormalize`, `CLAUDE.md` section 7).
- Un asset binaire n'est modifié que par **une seule branche à la fois**.
- Les agents commitent des fichiers **sources** (`.blend`, `.fbx`, textures) uniquement dans les branches `asset/`, jamais dans une branche `feature/`.
- Les `.uasset` sont créés ou modifiés par `bp-integrator` via le MCP Unreal, sur la branche `feature/` (ou `integ/` s'il n'y a pas de nouveau C++), uniquement pour les assets de la checklist. L'humain traite à la main les étapes [HUMAIN]. Un import par script Python libre reste exclu en v1.

### Worktrees git

Pour que deux agents travaillent en même temps sur deux branches, chaque branche active a son propre **worktree** (un dossier de travail séparé, qui partage le même dépôt git). Deux agents d'écriture ne travaillent jamais dans le même dossier.

Limite propre à Unreal à connaître : chaque worktree a ses propres dossiers `Intermediate/`, `Binaries/` et `Saved/`, et une compilation complète y est lourde en temps et en disque. Recommandation : **2 ou 3 worktrees actifs au maximum** au début.

---

## 7. Stratégie de test

| Framework | Usage dans ce projet |
|---|---|
| **Automation Spec** (défaut) | Logique pure : santé, recette de chaudron, conversion d'os, seuils de vagues |
| **Functional Test** | Seulement quand un monde vivant est indispensable (spawn de vague, serviteur errant dans la zone de base) |
| **Simple Automation Test** | Cas triviaux uniquement |

Les Automation Spec sont écrites en C++ pur, donc un agent peut les écrire et les corriger. Les Functional Tests exigent une map (`.umap`), que seul l'humain crée.

Comme Claude Code tourne en local, il peut lancer les tests en ligne de commande avec `UnrealEditor-Cmd.exe`. Aucun test n'existe encore dans le projet (état au 2026-11-10).

Commande **proposée, jamais lancée** :

```
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "C:\Projects\Unreal Projects\Spooky_Spores\Spooky_Spores.uproject" ^
  -ExecCmds="Automation RunTests <filtre>;Quit" -unattended -nullrhi -nosplash ^
  -ReportExportPath="<dossier hors dépôt>"
```

Ce qui est confirmé par la documentation officielle d'Epic (page marquée 5.9, donc pas spécifique à 5.8.3) : `-ExecCmds="Automation RunTest <tests>;Quit"` et `-ReportExportPath`. Ce qui ne l'est pas : `-unattended`, `-nullrhi`, `-nosplash` (sources de forums seulement), et l'orthographe `RunTest` (doc) contre `RunTests` (forums). Les noms de tests se découvrent avec `Automation List`. **À valider par un essai réel sur 5.8.3** avant de figer la procédure.

Règle de qualité : un test doit pouvoir échouer. Si un test ne peut pas échouer quand on casse le comportement, il est supprimé.

---

## 8. Autonomie et montée en confiance

Dans chaque branche de travail, l'ordre pour relâcher le contrôle, du moins risqué au plus risqué :

1. **Niveau 1 — contrôles automatisés** : lancement des tests, revue de code, vérification de cohérence. Ils ne cassent rien.
2. **Niveau 2 — implémentation automatisée** d'items petits et bien spécifiés, dans une branche, avec revue avant PR.
3. **Niveau 3 — enchaînement plus autonome** (plusieurs étapes sans pause), seulement après plusieurs cycles réussis.

**Deux points de validation humaine restent obligatoires au début** :
- après la spec (une erreur de conception automatisée se propage à tout le pipeline) ;
- avant la PR vers `dev`.

Ils pourront être assouplis item par item, jamais par défaut.

---

## 9. Garde-fous communs à tous les agents

- Commit et push autorisés **uniquement sur sa propre branche de travail**. Jamais de commit ni de push sur `dev` ou `main`, jamais de push forcé, jamais de fusion : les fusions vers `dev` restent à l'humain.
- Aucune fusion de PR par un agent (`git merge`, `gh pr merge` interdits) : l'humain fusionne.
- Rester dans son périmètre de fichiers : toute modification hors périmètre est signalée à l'orchestrateur, pas appliquée.
- **Avant toute action** (modification, commande, génération) : expliquer ce qui va être fait et comment. Cette règle de travail s'applique aussi aux sous-agents via leur prompt.
- Une délégation est **auto-suffisante** : objectif, extrait de GDD pertinent, fichiers concernés, contraintes, format de sortie attendu.
- Un agent qui échoue ou doute le dit explicitement, il ne comble pas les trous en inventant.
- Profondeur de délégation maximale : 1. Un sous-agent ne délègue pas à son tour.

---

## 10. Application technique des garde-fous

Les règles des sections précédentes ne sont pas toutes imposables par la configuration de Claude Code. Ce qui suit vient de la documentation officielle (code.claude.com/docs), consultée le 2026-11-10 par un agent de recherche et **non revérifiée page par page** ; chaque point marqué « à tester » doit l'être avant de s'y fier.

- **Branches protégées** : une règle locale `Bash(git push *)` en `deny` se contourne (par exemple `git -C . push`). La vraie barrière est le ruleset GitHub : `main` exige déjà une PR ; **`dev` doit l'exiger aussi** (réglage à faire par l'humain dans GitHub, voir section 11). En complément, un hook `PreToolUse` sur `git commit` / `git push` qui lit la branche courante (`git branch --show-current`) et refuse `dev` et `main` (`exit 2`). Un hook qui dépasse son délai laisse passer l'appel par défaut : à tester.
- **Périmètre de fichiers** : aucun champ du frontmatter d'un agent ne limite l'écriture à un dossier. Leviers : règles `Edit(chemin)` dans les permissions (les redirections `>` sont contrôlées, pas `cp` ni `mv`), hook `PreToolUse`, retrait de `Write`/`Edit` pour les agents en lecture seule. Ce n'est pas une frontière : **avant d'accepter le travail d'un agent, l'orchestrateur compare `git diff --name-only` au « Périmètre de fichiers » de la spec**. À tester : les règles `Edit` s'appliquent-elles bien aux sous-agents.
- **Profondeur de délégation** : la valeur par défaut est de 3 niveaux. Pour imposer la profondeur 1 de la section 9, définir `CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH=1`.
- **Worktrees** : par défaut, un worktree d'agent part de la branche par défaut du dépôt (`main`), pas de `dev`. Soit régler `worktree.baseRef` sur `"head"` depuis `dev`, soit créer les worktrees à la main avec `git worktree add` depuis `dev`. Un worktree n'a ni `Binaries/` ni `Intermediate/` (recompilation complète) et contient des pointeurs LFS tant que `git lfs pull` n'y est pas fait. Un sous-agent en worktree lit le `CLAUDE.md` du checkout principal. `.claude/worktrees/` doit être ajouté au `.gitignore`.
- **Éditeur et MCP Unreal** (état au 2026-11-10) : un seul éditeur, qui travaille sur le **checkout principal**, quelle que soit la branche — donc un seul `bp-integrator` à la fois, et l'humain ne touche pas à l'éditeur pendant son passage. Outils **exclus** en v1 (décision de l'humain, à reconsidérer plus tard) : `ue_python` (Python libre, hors de toute règle de permission), `ue_console_command`, `ue_delete_asset`, `ue_destroy_actor`, `ue_new_level`, `ue_open_level`, ainsi que `ue_play` et `ue_stop_play` (le test en jeu est humain). Conséquence : sans Python, pas de sauvegarde ciblée ; `bp-integrator` utilise `ue_save_all` (qui enregistre aussi des assets sans rapport) et **ne stage que les fichiers de la checklist** ; le reste est listé dans son rapport. `bp-integrator` vérifie avant de commencer la branche, le projet ouvert et la propreté de l'arbre de travail ; le commit de l'état précédent est fait avant son passage, donc un retour arrière est un retour au commit précédent, **éditeur fermé** (voir `CLAUDE.md` section 7). La compilation C++ n'est pas couverte par les outils : elle reste une étape humaine.
- **Liste d'outils des agents** : les sous-agents héritent des outils MCP de la session principale (d'après la doc consultée) ; chaque agent a donc une liste `tools` explicite, et seuls `bp-integration-planner` (lecture), `bp-integrator` (écriture restreinte) et `editor-verifier` (lecture) reçoivent des outils Unreal. À tester : que la liste blanche filtre bien les noms `mcp__unreal__*`.
- **Workflows scriptés** (`.claude/workflows/`) : ils n'acceptent aucune saisie humaine en cours de route ; une validation entre deux étapes impose deux workflows séparés. Ils ne sont pas retenus pour le moment : on démarre avec des agents et une table de routage simples.

---

## 11. Décisions ouvertes

1. **Périmètre de départ** : 8 rôles créés (voir le statut en tête). `asset-creator`, `level-designer` et `art-audio-checker` ne sont pas créés : aucun besoin concret aujourd'hui, à ajouter quand il apparaît (le MCP Blender est connecté, `asset-creator` serait le premier utile).
2. **Verrouillage LFS (`lockable`)** : l'API de verrous de GitHub répond (vérifié le 2026-11-10, aucun verrou actif) et `lockable` n'est défini nulle part. Recommandation : **pas maintenant**. Un fichier `lockable` est en lecture seule en local tant qu'il n'est pas verrouillé, et son comportement avec l'éditeur Unreal n'est pas vérifié. Si adopté un jour, ce serait un second filet pour la règle « un seul écrivain par binaire », sans remplacer la vérification de périmètre pour le C++ et la documentation.
3. **PR obligatoire sur `dev`** : le ruleset actuel de `dev` bloque seulement le force push et la suppression. Sans PR obligatoire, « aucune fusion sans l'humain » n'est qu'une discipline. Réglage GitHub à faire par l'humain.
4. **Procédure de lancement des tests** sous Unreal 5.8.3 : commande proposée en section 7, à valider par un essai réel. L'infrastructure de test existe (`Source/Spooky_Spores/Tests/`) et n'a pas exigé de modification de `Spooky_Spores.Build.cs`. Lancement validé : clic droit sur `Tests/` dans Rider puis « Run Unit Tests », ou panneau Automation de l'éditeur avec le filtre `SpookySpores`. Un monde de test est réalisable en Automation Spec (voir `Tests/BaseZoneWorldSpec.cpp`).
5. **MCP Blender** : connecté et vérifié le 2026-11-10 (Blender 5.0.1, add-on 1.8 à jour, protocole 15 ; bibliothèques d'assets, générateurs 3D et télémétrie désactivés). Déclaré au niveau global (`uvx blender-mcp`), **version non épinglée**. Le socket n'a pas d'authentification : rester en local. Les contraintes de format d'export (échelle, axes, nommage) seront fixées dans un brief type quand `asset-creator` sera créé.
6. **Import automatisé d'assets** par script Python de l'éditeur : à explorer, ou on reste en import manuel.
7. **Rôles supplémentaires** (son, narration, UI) : à ajouter quand un besoin réel apparaît.
8. **Worktrees parallèles** : démarrer en séquentiel, puis 2 ou 3 au maximum. À vérifier que `.claude/worktrees/` à l'intérieur du projet ne perturbe ni Unreal ni Rider. Avec un seul éditeur sur le checkout principal, l'intégration éditeur reste de toute façon séquentielle.
9. **MCP Unreal** : éditeur détecté et `ue_project_info` fonctionnel le 2026-11-10 (5.8.3, niveau `Lvl_Principal`) ; `ue_status` indiquait `connected: false` juste avant la première commande, la connexion semble s'établir à la demande (à reconfirmer). Déclaré globalement (`uvx --with "mcp<2" uemcp`), version non épinglée. **Aucun outil ne couvre l'Event Graph ni les widgets UMG** (non vu dans la liste d'outils ; `ue_python`, exclu, le permettrait peut-être en partie, non vérifié) : ces étapes restent [HUMAIN] et la conception doit garder la logique en C++. `EngineWire` est aussi chargé dans l'éditeur : deux ponts vers Unreal, à arbitrer. `bRemoteExecution=True` est dans `Config/DefaultEngine.ini` (réglage de projet).
10. **PR et `gh`** : `gh` 2.103.3 est installé (2026-11-10) et authentifié par l'humain (droits ADMIN sur le dépôt, jeton avec le scope `repo`). Ce jeton permet aussi `gh pr merge` : `.claude/settings.json` refuse `Bash(gh pr merge *)` et `Bash(gh api *)`. C'est contournable (appel par chemin complet, autre outil) : un garde-fou, pas une frontière. Le ruleset de `dev` n'empêche pas non plus un agent de fusionner sa propre PR tant que 0 approbation est exigée : reste à régler côté GitHub une PR obligatoire sur `dev`.

---

## Annexe A — Modèle de spec

Plafond : **100 lignes**. Au-delà, découper la feature. Les détails de test, les alternatives écartées et les vérifications dans le code moteur n'ont pas leur place ici (une ligne au plus).

**Si une décision de gameplay ou de direction est ouverte**, `game-designer` écrit d'abord une **note de cadrage** (~20 lignes, statut « cadrage ») : l'objectif et chaque décision au format « Voilà le problème → je propose X → qu'en penses-tu ? ». La spec suit une fois les décisions prises.

```markdown
# Spec — <item>
Item TODO : <numéro et titre> | Branche : feature/<nom> | Statut : cadrage | brouillon | validée | implémentée

## Objectif
2-3 lignes : ce que la mécanique apporte au joueur (lien avec le GDD). Dépendances éventuelles en une phrase.

## Comportement
Puces courtes. Les choix techniques tranchés par l'agent : une ligne chacun.

## Critères d'acceptation
10 au plus, une phrase chacun, vérifiables (chacun devient au moins un test).
- [ ] ...

## API exposée
Signatures UFUNCTION / UPROPERTY / événements attendus.

## Valeurs réglables
`Nom` : valeur par défaut (une ligne chacune).

## Cas limites
5 au plus, seulement les non évidents.

## Périmètre de fichiers
À créer / à modifier / points chauds touchés (à sérialiser).

## Hors périmètre
3-5 puces.
```
