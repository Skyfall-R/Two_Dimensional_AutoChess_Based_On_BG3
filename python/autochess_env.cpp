// Headless Python binding for the AutoChess engine.
//
// Two coexisting APIs:
//   1. Legacy global-engine API (reset / step / legal_actions / ...) for
//      backwards compatibility with the previous train_ai.py.
//   2. Handle-based API (env_create / env_clone / env_step / ...) used by
//      the new tools/training/alphazero pipeline. Each handle is an
//      independently-owned GameEngine, which lets MCTS fork environments
//      cheaply and lets a single Python process drive multiple parallel
//      rollouts when needed.

#define PY_SSIZE_T_CLEAN
#ifdef _DEBUG
#define AUTOCHESS_RESTORE_DEBUG
#undef _DEBUG
#endif
#include <Python.h>
#ifdef AUTOCHESS_RESTORE_DEBUG
#define _DEBUG
#undef AUTOCHESS_RESTORE_DEBUG
#endif

#include <lib.hpp>

#include <algorithm>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

using EnvHandle = int;

struct EnvSlot {
    std::unique_ptr<autochess::GameEngine> engine;
    int stepCount = 0;
    int maxSteps = 256;
    autochess::AiDifficulty difficulty = autochess::AiDifficulty::Hard;
    std::string policyDirectory = "assets/ai";
    unsigned seed = 1;
};

std::unordered_map<EnvHandle, EnvSlot> g_envs;
EnvHandle g_nextHandle = 1;
std::mutex g_handleMutex;

// Legacy global handle backing the old API.
EnvHandle g_legacyHandle = 0;

void setString(PyObject* dict, const char* key, const std::string& value) {
    PyObject* object = PyUnicode_FromString(value.c_str());
    PyDict_SetItemString(dict, key, object);
    Py_DECREF(object);
}

void setInt(PyObject* dict, const char* key, long value) {
    PyObject* object = PyLong_FromLong(value);
    PyDict_SetItemString(dict, key, object);
    Py_DECREF(object);
}

void setDouble(PyObject* dict, const char* key, double value) {
    PyObject* object = PyFloat_FromDouble(value);
    PyDict_SetItemString(dict, key, object);
    Py_DECREF(object);
}

void setBool(PyObject* dict, const char* key, bool value) {
    PyObject* object = PyBool_FromLong(value ? 1 : 0);
    PyDict_SetItemString(dict, key, object);
    Py_DECREF(object);
}

PyObject* doubleList(const std::vector<double>& values) {
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(values.size()));
    for (Py_ssize_t i = 0; i < static_cast<Py_ssize_t>(values.size()); ++i) {
        PyList_SET_ITEM(list, i, PyFloat_FromDouble(values[static_cast<size_t>(i)]));
    }
    return list;
}

EnvSlot* slotFor(EnvHandle handle, bool raise = true) {
    auto it = g_envs.find(handle);
    if (it == g_envs.end()) {
        if (raise) PyErr_SetString(PyExc_KeyError, "invalid env handle");
        return nullptr;
    }
    return &it->second;
}

EnvHandle createInternal(unsigned seed,
                         autochess::AiDifficulty difficulty,
                         const std::string& policyDirectory,
                         int maxSteps) {
    EnvSlot slot;
    slot.engine = std::make_unique<autochess::GameEngine>(seed);
    autochess::GameConfig config;
    config.mode = autochess::GameMode::SinglePlayerVsAi;
    config.aiDifficulty = difficulty;
    config.aiPolicyDirectory = policyDirectory;
    slot.engine->startNewGame(config);
    slot.engine->consumeEvents();
    slot.difficulty = difficulty;
    slot.policyDirectory = policyDirectory;
    slot.seed = seed;
    slot.stepCount = 0;
    slot.maxSteps = std::max(1, maxSteps);

    EnvHandle handle = g_nextHandle++;
    g_envs.emplace(handle, std::move(slot));
    return handle;
}

EnvHandle ensureLegacy() {
    if (g_legacyHandle == 0 || g_envs.find(g_legacyHandle) == g_envs.end()) {
        g_legacyHandle = createInternal(1, autochess::AiDifficulty::Hard, "assets/ai", 256);
    }
    return g_legacyHandle;
}

PyObject* snapshotDict(const autochess::GameEngine& engine) {
    autochess::GameSnapshot snapshot = engine.snapshot();
    PyObject* dict = PyDict_New();
    setInt(dict, "round", snapshot.round);
    setDouble(dict, "time", snapshot.time);
    setDouble(dict, "combatTime", snapshot.combatTime);
    setString(dict, "run", "Exploration");
    setInt(dict, "mapKind", static_cast<int>(snapshot.mapKind));
    setInt(dict, "explorationRound", snapshot.explorationRound);
    setInt(dict, "explorationRoundLimit", snapshot.explorationRoundLimit);
    setInt(dict, "explorationRoundsRemaining", snapshot.explorationRoundsRemaining);
    setBool(dict, "explorationRoundLimitLocked", snapshot.explorationRoundLimitLocked);
    setInt(dict, "explorationObjectivesCleared", snapshot.explorationObjectivesCleared);
    setInt(dict, "explorationObjectivesTotal", snapshot.explorationObjectivesTotal);
    setInt(dict, "bossesCleared", snapshot.bossesCleared);
    setInt(dict, "eventsTriggered", snapshot.eventsTriggered);
    setInt(dict, "trapsTriggered", snapshot.trapsTriggered);
    setInt(dict, "playerExplorationScore", snapshot.explorationScores[0]);
    setInt(dict, "enemyExplorationScore", snapshot.explorationScores[1]);
    setInt(dict, "playerBossesCleared", snapshot.explorationBossesClearedByPlayer[0]);
    setInt(dict, "enemyBossesCleared", snapshot.explorationBossesClearedByPlayer[1]);
    setString(dict, "phase", autochess::toString(snapshot.phase));
    setBool(dict, "done", snapshot.phase == autochess::Phase::Finished);
    setString(dict, "winner", snapshot.winner ? autochess::toString(*snapshot.winner) : "");
    setInt(dict, "playerMoney", snapshot.players[0].money);
    setInt(dict, "enemyMoney", snapshot.players[1].money);
    setInt(dict, "playerBench", static_cast<long>(snapshot.players[0].bench.size()));
    setInt(dict, "enemyBench", static_cast<long>(snapshot.players[1].bench.size()));
    setInt(dict, "playerDeployed", static_cast<long>(snapshot.players[0].deployed.size()));
    setInt(dict, "enemyDeployed", static_cast<long>(snapshot.players[1].deployed.size()));
    return dict;
}

PyObject* actionDict(autochess::GameEngine& engine,
                     const autochess::AiAction& action,
                     int index) {
    PyObject* dict = PyDict_New();
    setInt(dict, "index", index);
    setString(dict, "kind", autochess::toString(action.kind));
    setString(dict, "type", autochess::toString(action.type));
    setInt(dict, "typeId", static_cast<int>(action.type));
    setInt(dict, "kindId", static_cast<int>(action.kind));
    setInt(dict, "unitId", action.unitId);
    setInt(dict, "x", action.coord.x);
    setInt(dict, "y", action.coord.y);
    PyObject* features = doubleList(engine.actionFeatures(autochess::PlayerId::One, action));
    PyDict_SetItemString(dict, "features", features);
    Py_DECREF(features);
    return dict;
}

PyObject* legalActionsList(autochess::GameEngine& engine) {
    std::vector<autochess::AiAction> actions = engine.legalActions(autochess::PlayerId::One);
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(actions.size()));
    for (Py_ssize_t i = 0; i < static_cast<Py_ssize_t>(actions.size()); ++i) {
        PyList_SET_ITEM(list, i, actionDict(engine, actions[static_cast<size_t>(i)], static_cast<int>(i)));
    }
    return list;
}

double tickCombatTo(autochess::GameEngine& engine, double maxSeconds) {
    const double dt = 1.0 / 30.0;
    int maxTicks = std::max(1, static_cast<int>(maxSeconds / dt));
    double ticked = 0.0;
    for (int i = 0; i < maxTicks; ++i) {
        if (engine.snapshot().phase != autochess::Phase::Combat) break;
        engine.tick(dt);
        ticked += dt;
    }
    return ticked;
}

double stepReward(autochess::GameEngine& engine,
                  const autochess::GameSnapshot& before,
                  bool applied,
                  bool done,
                  std::optional<autochess::PlayerId> winner) {
    autochess::GameSnapshot after = engine.snapshot();
    int beforeScoreDelta = before.explorationScores[0] - before.explorationScores[1];
    int afterScoreDelta = after.explorationScores[0] - after.explorationScores[1];

    double reward = 0.0;
    if (!applied) reward -= 0.05;
    reward += static_cast<double>(afterScoreDelta - beforeScoreDelta) / 100.0;
    if (done && winner) {
        reward += *winner == autochess::PlayerId::One ? 1.0 : -1.0;
    }
    return reward;
}

// ---------------------------------------------------------------------------
// Handle-based API
// ---------------------------------------------------------------------------

PyObject* py_env_create(PyObject*, PyObject* args, PyObject* kwargs) {
    unsigned int seed = 1;
    int maxSteps = 256;
    const char* difficulty = "Hard";
    const char* policyDirectory = "assets/ai";
    static const char* keywords[] = {"seed", "max_steps", "difficulty", "policy_dir", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Iiss", const_cast<char**>(keywords),
                                     &seed, &maxSteps, &difficulty, &policyDirectory)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_handleMutex);
    EnvHandle handle = createInternal(
        seed,
        autochess::aiDifficultyFromString(difficulty ? difficulty : "Hard"),
        policyDirectory ? policyDirectory : "assets/ai",
        maxSteps);
    return PyLong_FromLong(handle);
}

PyObject* py_env_close(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    std::lock_guard<std::mutex> lock(g_handleMutex);
    g_envs.erase(handle);
    if (g_legacyHandle == handle) g_legacyHandle = 0;
    Py_RETURN_NONE;
}

PyObject* py_env_clone(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    std::lock_guard<std::mutex> lock(g_handleMutex);
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    EnvSlot copy;
    copy.engine = std::make_unique<autochess::GameEngine>(*slot->engine);
    copy.stepCount = slot->stepCount;
    copy.maxSteps = slot->maxSteps;
    copy.difficulty = slot->difficulty;
    copy.policyDirectory = slot->policyDirectory;
    copy.seed = slot->seed;
    EnvHandle newHandle = g_nextHandle++;
    g_envs.emplace(newHandle, std::move(copy));
    return PyLong_FromLong(newHandle);
}

PyObject* py_env_observation(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    return snapshotDict(*slot->engine);
}

PyObject* py_env_legal_actions(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    return legalActionsList(*slot->engine);
}

PyObject* py_env_state_features(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    return doubleList(slot->engine->stateFeatures(autochess::PlayerId::One));
}

PyObject* py_env_step(PyObject*, PyObject* args) {
    int handle = 0;
    int actionIndex = 0;
    if (!PyArg_ParseTuple(args, "ii", &handle, &actionIndex)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;

    autochess::GameEngine& engine = *slot->engine;
    autochess::GameSnapshot before = engine.snapshot();
    std::vector<autochess::AiAction> actions = engine.legalActions(autochess::PlayerId::One);

    bool applied = false;
    if (actionIndex >= 0 && actionIndex < static_cast<int>(actions.size())) {
        applied = engine.applyAiAction(autochess::PlayerId::One, actions[static_cast<size_t>(actionIndex)]);
    }

    // If our action transitioned to combat (Ready), simulate combat to completion.
    if (engine.snapshot().phase == autochess::Phase::Combat) {
        tickCombatTo(engine, 60.0);
    }
    engine.consumeEvents();

    autochess::GameSnapshot after = engine.snapshot();
    ++slot->stepCount;
    bool done = after.phase == autochess::Phase::Finished || slot->stepCount >= slot->maxSteps;
    double reward = stepReward(engine, before, applied, done, after.winner);

    PyObject* result = PyDict_New();
    PyObject* observation = snapshotDict(engine);
    PyDict_SetItemString(result, "observation", observation);
    Py_DECREF(observation);
    setDouble(result, "reward", reward);
    setBool(result, "done", done);
    setBool(result, "applied", applied);
    setInt(result, "step", slot->stepCount);
    PyObject* legal = legalActionsList(engine);
    PyDict_SetItemString(result, "legalActions", legal);
    Py_DECREF(legal);
    return result;
}

PyObject* py_env_finalize_round(PyObject*, PyObject* args) {
    // Ensure combat (if any) is fully resolved. Useful when MCTS tree
    // terminates at a leaf where Ready has just been issued.
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    if (slot->engine->snapshot().phase == autochess::Phase::Combat) {
        tickCombatTo(*slot->engine, 60.0);
    }
    slot->engine->consumeEvents();
    return snapshotDict(*slot->engine);
}

PyObject* py_env_set_difficulty(PyObject*, PyObject* args) {
    int handle = 0;
    const char* difficulty = "Hard";
    if (!PyArg_ParseTuple(args, "is", &handle, &difficulty)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    autochess::AiDifficulty parsed = autochess::aiDifficultyFromString(difficulty ? difficulty : "Hard");
    slot->engine->setAiDifficulty(parsed);
    slot->difficulty = parsed;
    Py_RETURN_NONE;
}

PyObject* py_env_set_policy_dir(PyObject*, PyObject* args) {
    int handle = 0;
    const char* directory = "assets/ai";
    if (!PyArg_ParseTuple(args, "is", &handle, &directory)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    slot->engine->setAiPolicyDirectory(directory ? directory : "assets/ai");
    slot->policyDirectory = directory ? directory : "assets/ai";
    Py_RETURN_NONE;
}

PyObject* py_env_rules_fingerprint(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    return PyUnicode_FromString(slot->engine->rulesFingerprint().c_str());
}

PyObject* py_env_feature_schema(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    autochess::AiFeatureSchema schema = slot->engine->aiFeatureSchema();
    PyObject* dict = PyDict_New();
    setInt(dict, "stateFeatureCount", schema.stateFeatureCount);
    setInt(dict, "actionFeatureCount", schema.actionFeatureCount);
    PyObject* stateGroups = PyList_New(static_cast<Py_ssize_t>(schema.stateFeatureGroups.size()));
    for (Py_ssize_t i = 0; i < static_cast<Py_ssize_t>(schema.stateFeatureGroups.size()); ++i) {
        PyList_SET_ITEM(stateGroups, i, PyUnicode_FromString(schema.stateFeatureGroups[static_cast<size_t>(i)].c_str()));
    }
    PyObject* actionGroups = PyList_New(static_cast<Py_ssize_t>(schema.actionFeatureGroups.size()));
    for (Py_ssize_t i = 0; i < static_cast<Py_ssize_t>(schema.actionFeatureGroups.size()); ++i) {
        PyList_SET_ITEM(actionGroups, i, PyUnicode_FromString(schema.actionFeatureGroups[static_cast<size_t>(i)].c_str()));
    }
    PyDict_SetItemString(dict, "stateFeatureGroups", stateGroups);
    PyDict_SetItemString(dict, "actionFeatureGroups", actionGroups);
    Py_DECREF(stateGroups);
    Py_DECREF(actionGroups);
    return dict;
}

PyObject* py_env_unit_catalog(PyObject*, PyObject* args) {
    int handle = 0;
    if (!PyArg_ParseTuple(args, "i", &handle)) return nullptr;
    EnvSlot* slot = slotFor(handle);
    if (!slot) return nullptr;
    const std::vector<autochess::UnitSpec>& specs = slot->engine->shop();
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(specs.size()));
    for (Py_ssize_t i = 0; i < static_cast<Py_ssize_t>(specs.size()); ++i) {
        const autochess::UnitSpec& spec = specs[static_cast<size_t>(i)];
        PyObject* item = PyDict_New();
        setInt(item, "typeId", static_cast<int>(spec.type));
        setString(item, "type", autochess::toString(spec.type));
        setString(item, "name", spec.name);
        setString(item, "shortName", spec.shortName);
        setInt(item, "cost", spec.cost);
        setInt(item, "unitCount", spec.unitCount);
        setInt(item, "maxHp", spec.maxHp);
        setInt(item, "attack", spec.attack);
        setInt(item, "range", spec.range);
        setInt(item, "armorClass", spec.armorClass);
        setInt(item, "attackBonus", spec.attackBonus);
        setInt(item, "savingThrowBonus", spec.savingThrowBonus);
        setInt(item, "spellSaveDc", spec.spellSaveDc);
        setDouble(item, "speed", spec.speed);
        setString(item, "layer", autochess::toString(spec.layer));
        setBool(item, "canAttackAir", spec.canAttackAir);
        setInt(item, "threat", spec.threat);
        PyList_SET_ITEM(list, i, item);
    }
    return list;
}

// ---------------------------------------------------------------------------
// Legacy global-engine API (delegates to a single hidden handle).
// Kept for the previous train_ai.py and any external callers.
// ---------------------------------------------------------------------------

PyObject* py_reset(PyObject*, PyObject* args, PyObject* kwargs) {
    unsigned int seed = 1;
    int maxSteps = 256;
    const char* difficulty = "Hard";
    const char* policyDirectory = "assets/ai";
    static const char* keywords[] = {"seed", "max_steps", "difficulty", "policy_dir", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Iiss", const_cast<char**>(keywords),
                                     &seed, &maxSteps, &difficulty, &policyDirectory)) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(g_handleMutex);
    if (g_legacyHandle != 0) g_envs.erase(g_legacyHandle);
    g_legacyHandle = createInternal(
        seed,
        autochess::aiDifficultyFromString(difficulty ? difficulty : "Hard"),
        policyDirectory ? policyDirectory : "assets/ai",
        maxSteps);
    EnvSlot* slot = slotFor(g_legacyHandle);
    return snapshotDict(*slot->engine);
}

PyObject* py_legal_actions(PyObject*, PyObject*) {
    EnvSlot* slot = slotFor(ensureLegacy());
    return legalActionsList(*slot->engine);
}

PyObject* py_state_features(PyObject*, PyObject*) {
    EnvSlot* slot = slotFor(ensureLegacy());
    return doubleList(slot->engine->stateFeatures(autochess::PlayerId::One));
}

PyObject* py_action_features(PyObject*, PyObject* args) {
    int index = 0;
    if (!PyArg_ParseTuple(args, "i", &index)) return nullptr;
    EnvSlot* slot = slotFor(ensureLegacy());
    std::vector<autochess::AiAction> actions = slot->engine->legalActions(autochess::PlayerId::One);
    if (index < 0 || index >= static_cast<int>(actions.size())) {
        PyErr_SetString(PyExc_IndexError, "action index out of range");
        return nullptr;
    }
    return doubleList(slot->engine->actionFeatures(autochess::PlayerId::One, actions[static_cast<size_t>(index)]));
}

PyObject* py_step(PyObject*, PyObject* args) {
    int actionIndex = 0;
    if (!PyArg_ParseTuple(args, "i", &actionIndex)) return nullptr;
    PyObject* tuple = Py_BuildValue("(ii)", static_cast<int>(ensureLegacy()), actionIndex);
    PyObject* result = py_env_step(nullptr, tuple);
    Py_DECREF(tuple);
    return result;
}

PyObject* py_rules_fingerprint(PyObject*, PyObject*) {
    EnvSlot* slot = slotFor(ensureLegacy());
    return PyUnicode_FromString(slot->engine->rulesFingerprint().c_str());
}

PyObject* py_feature_schema(PyObject*, PyObject*) {
    PyObject* tuple = Py_BuildValue("(i)", static_cast<int>(ensureLegacy()));
    PyObject* result = py_env_feature_schema(nullptr, tuple);
    Py_DECREF(tuple);
    return result;
}

PyObject* py_unit_catalog(PyObject*, PyObject*) {
    PyObject* tuple = Py_BuildValue("(i)", static_cast<int>(ensureLegacy()));
    PyObject* result = py_env_unit_catalog(nullptr, tuple);
    Py_DECREF(tuple);
    return result;
}

PyMethodDef kMethods[] = {
    // Handle-based API.
    {"env_create", reinterpret_cast<PyCFunction>(py_env_create), METH_VARARGS | METH_KEYWORDS,
     "Create a new env handle. Returns int handle id."},
    {"env_close", py_env_close, METH_VARARGS, "Close an env handle and release its engine."},
    {"env_clone", py_env_clone, METH_VARARGS, "Deep-copy an env into a new handle (used by MCTS)."},
    {"env_observation", py_env_observation, METH_VARARGS, "Snapshot dict for an env handle."},
    {"env_legal_actions", py_env_legal_actions, METH_VARARGS, "Legal actions for an env handle."},
    {"env_state_features", py_env_state_features, METH_VARARGS, "State feature vector for an env handle."},
    {"env_step", py_env_step, METH_VARARGS, "Apply legal action index for an env handle."},
    {"env_finalize_round", py_env_finalize_round, METH_VARARGS, "Run combat to completion on an env handle."},
    {"env_set_difficulty", py_env_set_difficulty, METH_VARARGS, "Switch AI difficulty on an env handle."},
    {"env_set_policy_dir", py_env_set_policy_dir, METH_VARARGS, "Override policy directory on an env handle."},
    {"env_rules_fingerprint", py_env_rules_fingerprint, METH_VARARGS, "Rules fingerprint for an env handle."},
    {"env_feature_schema", py_env_feature_schema, METH_VARARGS, "Feature schema for an env handle."},
    {"env_unit_catalog", py_env_unit_catalog, METH_VARARGS, "Unit catalog for an env handle."},

    // Legacy global API.
    {"reset", reinterpret_cast<PyCFunction>(py_reset), METH_VARARGS | METH_KEYWORDS,
     "Reset the legacy global engine."},
    {"legal_actions", py_legal_actions, METH_NOARGS, "Legacy: legal actions on the global engine."},
    {"step", py_step, METH_VARARGS, "Legacy: apply action on the global engine."},
    {"rules_fingerprint", py_rules_fingerprint, METH_NOARGS, "Legacy: rules fingerprint."},
    {"feature_schema", py_feature_schema, METH_NOARGS, "Legacy: feature schema."},
    {"state_features", py_state_features, METH_NOARGS, "Legacy: current state features."},
    {"action_features", py_action_features, METH_VARARGS, "Legacy: features for an action index."},
    {"unit_catalog", py_unit_catalog, METH_NOARGS, "Legacy: unit catalog."},
    {nullptr, nullptr, 0, nullptr}
};

PyModuleDef kModule = {
    PyModuleDef_HEAD_INIT,
    "autochess_env",
    "Headless AutoChess training environment backed by autochess_core. "
    "Provides both a global-engine legacy API and a handle-based API for MCTS.",
    -1,
    kMethods
};

} // namespace

PyMODINIT_FUNC PyInit_autochess_env() {
    return PyModule_Create(&kModule);
}
