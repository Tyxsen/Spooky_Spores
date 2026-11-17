# Workflow d'agents — Spooky_Spores

Comment un orchestrateur (la session Claude Code principale) délègue à des sous-agents spécialisés, comment le travail est isolé en branches git, et où l'humain valide. Document pour l'humain et l'orchestrateur : **aucun sous-agent n'a besoin de le lire**. Règles opérationnelles : `CLAUDE.md` section 10. Le jeu : `GDD.md`. Le code : `documentation-technique.md`. Le backlog : `TODO.md`.

**Agents** (`.claude/agents/`) : `game-designer`, `cpp-implementer`, `qa-test-writer`, `code-reviewer`, `consistency-keeper`, `bp-integration-planner`, `bp-integrator`, `editor-verifier`. Pas encore créés (à ajouter si un besoin réel apparaît) : `asset-creator` (Blender), `level-designer`, `art-audio-checker`. Moteur : Unreal Engine 5.8.3.

---

## 1. Principes

1. **Un agent = un rôle, une sortie, des droits limités.**
2. **Une unité de travail = une branche** (section 4).
3. **La spec est le contrat.** Un sous-agent ne voit pas la conversation : il ne connaît que ce qu'on lui transmet (`docs/specs/<item>.md`, ≤ 100 lignes). Les décisions de l'humain sont citées **mot pour mot** dans les briefs.
4. **Un seul écrivain par fichier à un instant donné.** Les binaires (`.uasset`, `.umap`, `.blend`) ne fusionnent pas.
5. **L'humain arbitre la conception.** Les agents proposent, l'humain valide aux arrêts obligatoires.
6. **Contrôles indépendants** : ceux qui vérifient (tests, revue, cohérence, éditeur) n'ont pas écrit ce qu'ils vérifient.

**Frontière agents / humain**
- Les agents produisent : specs, C++, tests, checklists, documentation ; et, par `bp-integrator` seul, l'intégration éditeur via le MCP Unreal (Blueprints enfants, composants, valeurs par défaut, placement), limitée aux étapes [MCP] d'une checklist.
- L'humain fait : valider la spec, **compiler** (fermer l'éditeur, rebuild dans Rider, rouvrir sur la branche), lancer les Automation Specs, ce que les outils ne couvrent pas (Event Graph, widgets UMG, éclairage, level design), le **test PIE**, **ouvrir la PR et fusionner**.
- Seul `bp-integrator` écrit des `.uasset`. L'implémenteur expose une API propre (`BlueprintCallable`, `EditAnywhere`, `BlueprintImplementableEvent`) pour que l'éditeur se réduise à du câblage.

---

## 2. Les rôles

| Rôle | Mission | Produit | Écrit dans |
|---|---|---|---|
| `game-designer` | Item du TODO → spec (ou note de cadrage si une décision est ouverte) | `docs/specs/<item>.md` | `docs/specs/` |
| `cpp-implementer` | Implémente la spec en C++ | Code | `Source/` |
| `qa-test-writer` | Tests **depuis la spec seule**, sans lire l'implémentation | Automation Specs | `Source/Spooky_Spores/Tests/` |
| `code-reviewer` | Relit le diff : bugs, conformité, pièges UE | Rapport (lecture seule) | — |
| `consistency-keeper` | Contradictions entre GDD, TODO, doc technique, README, `CLAUDE.md`, code ; met à jour les docs en fin de branche sur ordre | Rapport / docs | docs, sur ordre |
| `bp-integration-planner` | Checklist éditeur : étapes [MCP] / [HUMAIN], scénario de test | `docs/integration/<item>.md` | `docs/integration/` |
| `bp-integrator` | Exécute les étapes [MCP] dans l'éditeur ; ne commite pas | Assets, acteurs ; rapport dans sa réponse | Éditeur (MCP restreint) |
| `editor-verifier` | Relit l'état réel de l'éditeur vs la checklist (OK / KO / non vérifié) | Rapport (lecture seule) | — |

**Pourquoi le testeur ne voit pas l'implémentation** : un agent qui écrit le code puis ses tests écrit presque toujours des tests qui valident ce qu'il vient de faire, pas ce qui était voulu. `qa-test-writer` part de la spec : il teste le comportement attendu, cas limites compris, et chaque test doit pouvoir échouer (sinon il est supprimé). Pas de test pour le plaisir d'avoir un test.

**Routage** : voir le tableau de `CLAUDE.md` section 10 (nouvelle mécanique, bug C++, câblage d'un asset, valeur d'équilibrage). L'orchestrateur ne mobilise que les agents utiles. Les spécificités (modèle, outils) sont dans le frontmatter de chaque agent.

---

## 3. Le pipeline d'une mécanique

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
                                                         ║ HUMAIN n°2 : compile (ferme l'éditeur, rebuild dans  ║
                                                         ║   Rider, rouvre sur la branche), puis lance les      ║
                                                         ║   Automation Specs (clic droit sur Tests/ > Run Unit ║
                                                         ║   Tests) : tout vert avant de continuer              ║
                                                         ╚═══════════════════════════╤══════════════════════════╝
                                                                                     ▼
                                                       bp-integrator (préconditions, étapes [MCP]) — sauté si < 5 étapes [MCP] :
                                                                                     │   l'humain les fait lui-même
                                                                                     ▼
                                                                  editor-verifier (lecture seule, OK/KO/non vérifié)
                                                                                     ▼
                                                         ╔════════════════════════════════════════════════════════════╗
                                                         ║ HUMAIN n°3 : teste en éditeur (PIE) + mécaniques impactées ║
                                                         ╚═════════════╤════════════════════════════╤═════════════════╝
                                                              non : retours (cpp-implementer       oui
                                                             ou bp-integrator)                      ▼
                                                                                     consistency-keeper (met à jour les docs)
                                                                                                    ▼
                                                                        suppression de la spec et de la checklist (dernier commit)
                                                                                                    ▼
                                                           ╔════════════════════════════════════════════════════════════╗
                                                           ║ HUMAIN n°4 : go pour le push ; il ouvre la PR et fusionne  ║
                                                           ╚════════════════════════════════════════════════════════════╝
```

La note de cadrage n'est pas un arrêt de plus : c'est un point de décision à l'intérieur de l'arrêt n°1. Seules les décisions de gameplay ou de direction sont posées à l'humain, sous la forme « problème → proposition → qu'en penses-tu ? » ; les choix purement techniques sont tranchés par l'agent et notés en une ligne. Le parallélisme n'existe que là où il est sûr : les trois contrôles après les tests, et des unités de travail indépendantes sur des branches séparées.

---

## 4. Branches

`main` ← `dev` ← branches de travail. Aucun agent ne commite sur `dev` ou `main` ; `main` est à l'humain seul.

| Préfixe | Contenu | Exemple |
|---|---|---|
| `feature/` | C++ d'une mécanique, ses tests **et** son intégration éditeur (une branche, du C++ au test humain) | `feature/base-zone` |
| `integ/` | Intégration éditeur sans nouveau C++ (câblage, import d'un asset existant) | `integ/servant-skeleton` |
| `fix/` | Correction de bug | `fix/cauldron-recipe-null` |
| `docs/`, `chore/` | Documentation, outillage | `docs/sync-hp-values` |

(`asset/` et `level/` existeront avec `asset-creator` et `level-designer`.)

**Nommage et commits** : `<préfixe>/<item-en-kebab-case>`. Messages **en anglais** `[ADD]`, `[FIX]`, `[CHORE]`, `[DOC]` (`CLAUDE.md` section 7), **sans ligne d'attribution** (commits comme PR, sous-agents compris). Un commit = une intention.

**Cycle de vie**
1. L'orchestrateur crée la branche depuis `dev`.
2. Les agents commitent sur cette branche, dans leur périmètre de fichiers. Les contrôles tournent dessus.
3. Avant la PR, l'orchestrateur supprime `docs/specs/<item>.md` et `docs/integration/<item>.md` (temporaires) dans le dernier commit. Durables : le code, les tests, la doc technique.
4. Sur le go de l'humain, **l'orchestrateur pousse la branche** ; **l'humain ouvre la PR** vers `dev` et la fusionne en **Squash** (l'historique intermédiaire des agents n'est pas conservé). La branche est supprimée ensuite.

**Éviter les conflits**
- Chaque spec liste un **périmètre de fichiers**. Avant d'accepter le travail d'un agent, l'orchestrateur compare `git diff --name-only` à ce périmètre ; avant deux branches parallèles, il vérifie qu'elles ne se recoupent pas.
- **Points chauds, toujours sérialisés** : `EResourceType`, `*.Build.cs`, `Default*.ini`, `TODO.md`, `CLAUDE.md`, `documentation-technique.md`, `Lvl_Principal.umap`. Seul `consistency-keeper` met à jour les docs partagés, en fin de branche.
- **Binaires et LFS** (`CLAUDE.md` section 7) : un asset binaire n'est modifié que par une branche à la fois. Les `.uasset` sont créés par `bp-integrator` sur la branche `feature/` (ou `integ/`), uniquement pour les assets de la checklist.
- **Worktrees** : une branche active = un dossier de travail séparé ; deux agents d'écriture ne partagent jamais le même. Chaque worktree a son `Intermediate/`, `Binaries/` et `Saved/` (compilation complète lourde) : 2 ou 3 au maximum. Par défaut un worktree d'agent part de `main` (régler `worktree.baseRef` ou créer à la main depuis `dev`) et un sous-agent y lit le `CLAUDE.md` du checkout principal. Un seul éditeur existe de toute façon, donc l'intégration éditeur reste séquentielle.

---

## 5. Tests

**Automation Spec** (par défaut, C++ pur, un agent peut les écrire) pour la logique ; **Functional Test** seulement quand un monde vivant avec une map est indispensable (la map est créée par l'humain) ; Simple Automation Test pour le trivial. Un monde de test reste réalisable en Automation Spec (`Tests/BaseZoneWorldSpec.cpp`). Lancement : clic droit sur `Tests/` dans Rider > *Run Unit Tests*, ou panneau Automation de l'éditeur, filtre `SpookySpores`. Les tests existants sont **relancés** à chaque feature, pas réécrits : ils servent de filet de non-régression automatique.

---

## 6. Garde-fous communs

- Un agent commite **uniquement sur sa branche de travail** ; il ne pousse pas (l'orchestrateur pousse). Jamais de commit sur `dev`/`main`, de push forcé ou de fusion (`git merge`, `gh pr merge`).
- Il reste dans son périmètre de fichiers : le reste est signalé, pas appliqué.
- **Avant toute action**, il explique ce qu'il va faire (3 lignes au plus pour les agents).
- Une délégation est **auto-suffisante** : objectif, fichiers, contraintes, format de sortie, décisions de l'humain verbatim.
- Un agent qui doute le dit, il n'invente pas ; il ne délègue pas à un autre agent (profondeur 1).

### Ce que la configuration impose, et ce qu'elle n'impose pas
Sources : documentation Claude Code (code.claude.com/docs), consultée le 2026-11-10, non revérifiée page par page.
- **Branches protégées** : `deny` sur `git push` se contourne. La vraie barrière est le ruleset GitHub (`main` exige déjà une PR ; **`dev` devrait l'exiger aussi**, réglage à faire par l'humain). `.claude/settings.json` refuse `gh pr merge` et `gh api` : garde-fou, pas frontière.
- **Périmètre de fichiers** : aucun champ du frontmatter ne limite l'écriture à un dossier : c'est la comparaison de `git diff --name-only` qui fait foi.
- **Profondeur de délégation** : 3 par défaut ; `CLAUDE_CODE_MAX_SUBAGENT_SPAWN_DEPTH=1` pour imposer 1.
- **MCP Unreal** : un seul éditeur, sur le checkout principal, donc un seul `bp-integrator` à la fois et l'humain ne touche pas l'éditeur pendant son passage. Le serveur `unreal-engine` (`npx unreal-engine-mcp-server`, déclaré globalement dans `.claude.json`) expose **un seul outil**, `mcp__unreal-engine__unreal`, en quatre opérations (`search`, `describe`, `execute`, `configure`) ; il parle au serveur MCP natif d'EngineWire (port 3000, `Project Settings > EngineWire > Enable Native MCP Server`). L'ancien serveur `uemcp` et son réglage `UEMCP_MULTICAST_BIND` ne sont plus utilisés. L'éditeur doit être lancé : sinon tout appel répond `NOT_CONNECTED`.
  - **Pourquoi un hook** : avec un seul outil, la liste `tools` ne sépare plus lecture et écriture, et le champ `effect` du serveur n'est pas fiable (`set_camera` et `set_viewport_resolution` y sont `read`). Chaque agent déclare donc dans son frontmatter un hook `PreToolUse` qui lance `.claude/hooks/guard_unreal.py <profil>` : liste blanche d'identifiants de capacité (`read` pour planner et verifier, `write` pour integrator), refus par défaut, avec contraintes de paramètres (`blueprint.edit_variable` limité à `set_default`, `control_actor.edit_component` sans `remove`, `control_editor.save_all` avec `assetPaths` obligatoire). Le `consentGrant` du serveur n'est pas une barrière : l'agent le récupère lui-même.
  - **Exclus de `bp-integrator`** : Python, console, `call_editor_tool`, suppressions, création et ouverture de niveau, lancement du jeu (PIE), tests et build, réglages de projet (`Default*.ini`), Level Blueprint, logique de graphe (`edit_graph`, `add_behaviour`) et bindings de widgets (`bind_widget`, qui crée des fonctions), `add_game_widget` (contenu inconnu).
  - **Autorisé** : la **structure** des widgets (panneaux, contenu, mise en page) est [MCP] ; leur logique reste [HUMAIN], donc la logique reste en C++.
  - **Sauvegarde ciblée** : `control_editor.save_all` accepte `assetPaths` : `bp-integrator` n'enregistre que ce que la checklist touche. `git status` reste le contrôle des fichiers hors checklist.
  - **Joignabilité** : `system_control.get_project_settings` répond avec `projectName` = `Spooky_Spores` et un `projectDir` qui se termine par `/Spooky_Spores/` (le chemin change d'une machine à l'autre : ne pas le comparer en entier).
- **Liste d'outils** : chaque agent a une liste `tools` explicite ; seuls `bp-integration-planner` (lecture), `bp-integrator` (écriture restreinte) et `editor-verifier` (lecture) reçoivent l'outil Unreal. Les hooks du frontmatter ne tournent qu'après acceptation du dialogue de confiance du dossier.
- **Modèle** : `editor-verifier` et `consistency-keeper` tournent sur Sonnet (tâches de comparaison), les autres héritent du modèle de la session.
- **Retour arrière** : le commit précédent est fait avant le passage de `bp-integrator` ; un retour arrière est un retour à ce commit, éditeur fermé.

---

## 7. Points ouverts

1. **PR obligatoire sur `dev`** (GitHub, par l'humain) : sans elle, « aucune fusion sans l'humain » n'est qu'une discipline.
2. **MCP Blender** : connecté (déclaré globalement, version non épinglée, socket sans authentification : rester en local). Les contraintes d'export (échelle, axes, nommage) seront fixées quand `asset-creator` sera créé.
3. **Import d'assets** : la capacité `asset.import` (fichier sous le dossier du projet) est autorisée à `bp-integrator`, sans Python. Non éprouvée sur un asset réel (échelle, axes, nommage à fixer avec `asset-creator`).
4. **`EngineWire`** : le MCP Unreal passe désormais par son serveur natif, il n'y a plus deux ponts à arbitrer.
5. **MCP Unreal non épinglé** : `npx -y unreal-engine-mcp-server` télécharge la dernière version à chaque lancement, or l'outil écrit dans les `.uasset`. Épingler une version dans `.claude.json` (`unreal-engine-mcp-server@x.y.z`), à décider par l'humain.
6. **Garde-fou du MCP Unreal : ce qui est prouvé** (2026-11-17). Le script est testé en isolation (44 cas, mutations détectées). Une sonde temporaire a montré que le hook du frontmatter **s'exécute bien dans un sous-agent** (`editor-verifier`) : l'événement porte `agent_type`, `hook_event_name: PreToolUse` et un `tool_input` conforme (`operation`, `tool`, `action`, `capability`, `params`) ; un appel autorisé (`get_project_settings`) est passé. **Non constaté** : un refus réel via un sous-agent (le classifieur de permissions refuse qu'un agent tente un appel interdit, même inoffensif). **Le hook est ouvert par défaut en cas de panne** : seul le code de sortie 2 bloque ; une erreur de syntaxe, un `python` absent du PATH ou un plantage sort en 1 ou 127 et laisse passer l'appel (le script intercepte ses propres exceptions, pas ces cas-là). Après toute modification de `guard_unreal.py`, lancer `python -I .claude/hooks/test_guard_unreal.py` (il échoue si le script est cassé).
7. **LFS `lockable`** : non adopté (comportement avec l'éditeur Unreal non vérifié). Ce serait un second filet pour la règle « un seul écrivain par binaire ».
8. **Autonomie** : on relâche le contrôle progressivement (contrôles automatisés → implémentation d'items petits → enchaînement sans pause), jamais par défaut. Les 4 arrêts obligatoires s'assouplissent item par item après plusieurs cycles réussis.
