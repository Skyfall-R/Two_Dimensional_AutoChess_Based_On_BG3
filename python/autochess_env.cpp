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
#include <string>
#include <vector>

namespace {

std::unique_ptr<autochess::GameEngine> g_engine;
int g_stepCount = 0;
int g_maxSteps = 256;

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

autochess::GameEngine& engine() {
    if (!g_engine) {
        g_engine = std::make_unique<autochess::GameEngine>(1);
        autochess::GameConfig config;
        config.mode = autochess::GameMode::SinglePlayerVsAi;
        g_engine->startNewGame(config);
        g_engine->consumeEvents();
    }
    return *g_engine;
}

int towerHp(const autochess::GameSnapshot& snapshot, autochess::PlayerId player) {
    int total = 0;
    for (const autochess::UnitView& unit : snapshot.units) {
        if (unit.owner == player && unit.type == autochess::UnitType::DefenseTower && unit.alive) {
            total += unit.totalHp;
        }
    }
    return total;
}

PyObject* snapshotDict() {
    autochess::GameSnapshot snapshot = engine().snapshot();
    PyObject* dict = PyDict_New();
    setInt(dict, "round", snapshot.round);
    setDouble(dict, "time", snapshot.time);
    setDouble(dict, "combatTime", snapshot.combatTime);
    setString(dict, "phase", autochess::toString(snapshot.phase));
    setBool(dict, "done", snapshot.phase == autochess::Phase::Finished);
    setString(dict, "winner", snapshot.winner ? autochess::toString(*snapshot.winner) : "");
    setInt(dict, "playerMoney", snapshot.players[0].money);
    setInt(dict, "enemyMoney", snapshot.players[1].money);
    setInt(dict, "playerBench", static_cast<long>(snapshot.players[0].bench.size()));
    setInt(dict, "enemyBench", static_cast<long>(snapshot.players[1].bench.size()));
    setInt(dict, "playerTowerHp", towerHp(snapshot, autochess::PlayerId::One));
    setInt(dict, "enemyTowerHp", towerHp(snapshot, autochess::PlayerId::Two));
    return dict;
}

PyObject* actionDict(const autochess::AiAction& action, int index) {
    PyObject* dict = PyDict_New();
    setInt(dict, "index", index);
    setString(dict, "kind", autochess::toString(action.kind));
    setString(dict, "type", autochess::toString(action.type));
    setInt(dict, "typeId", static_cast<int>(action.type));
    setInt(dict, "unitId", action.unitId);
    setInt(dict, "x", action.coord.x);
    setInt(dict, "y", action.coord.y);
    PyObject* features = doubleList(engine().actionFeatures(autochess::PlayerId::One, action));
    PyDict_SetItemString(dict, "features", features);
    Py_DECREF(features);
    return dict;
}

PyObject* legalActionsList() {
    std::vector<autochess::AiAction> actions = engine().legalActions(autochess::PlayerId::One);
    PyObject* list = PyList_New(static_cast<Py_ssize_t>(actions.size()));
    for (Py_ssize_t i = 0; i < static_cast<Py_ssize_t>(actions.size()); ++i) {
        PyList_SET_ITEM(list, i, actionDict(actions[static_cast<size_t>(i)], static_cast<int>(i)));
    }
    return list;
}

PyObject* py_reset(PyObject*, PyObject* args, PyObject* kwargs) {
    unsigned int seed = 1;
    int maxSteps = 256;
    const char* difficulty = "Normal";
    const char* policyDirectory = "assets/ai";
    static const char* keywords[] = {"seed", "max_steps", "difficulty", "policy_dir", nullptr};
    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|Iiss", const_cast<char**>(keywords),
                                     &seed, &maxSteps, &difficulty, &policyDirectory)) {
        return nullptr;
    }

    g_engine = std::make_unique<autochess::GameEngine>(seed);
    autochess::GameConfig config;
    config.mode = autochess::GameMode::SinglePlayerVsAi;
    config.aiDifficulty = autochess::aiDifficultyFromString(difficulty ? difficulty : "Normal");
    config.aiPolicyDirectory = policyDirectory ? policyDirectory : "assets/ai";
    g_engine->startNewGame(config);
    g_engine->consumeEvents();
    g_stepCount = 0;
    g_maxSteps = std::max(1, maxSteps);
    return snapshotDict();
}

PyObject* py_legal_actions(PyObject*, PyObject*) {
    return legalActionsList();
}

PyObject* py_state_features(PyObject*, PyObject*) {
    return doubleList(engine().stateFeatures(autochess::PlayerId::One));
}

PyObject* py_action_features(PyObject*, PyObject* args) {
    int index = 0;
    if (!PyArg_ParseTuple(args, "i", &index)) return nullptr;
    std::vector<autochess::AiAction> actions = engine().legalActions(autochess::PlayerId::One);
    if (index < 0 || index >= static_cast<int>(actions.size())) {
        PyErr_SetString(PyExc_IndexError, "action index out of range");
        return nullptr;
    }
    return doubleList(engine().actionFeatures(autochess::PlayerId::One, actions[static_cast<size_t>(index)]));
}

PyObject* py_step(PyObject*, PyObject* args) {
    int index = 0;
    if (!PyArg_ParseTuple(args, "i", &index)) return nullptr;

    autochess::GameSnapshot before = engine().snapshot();
    int beforeSelfTower = towerHp(before, autochess::PlayerId::One);
    int beforeEnemyTower = towerHp(before, autochess::PlayerId::Two);
    std::vector<autochess::AiAction> actions = engine().legalActions(autochess::PlayerId::One);

    double reward = 0.0;
    bool applied = false;
    if (index >= 0 && index < static_cast<int>(actions.size())) {
        applied = engine().applyAiAction(autochess::PlayerId::One, actions[static_cast<size_t>(index)]);
    }
    if (!applied) reward -= 0.05;

    for (int ticks = 0; ticks < 30 * 60 && engine().snapshot().phase == autochess::Phase::Combat; ++ticks) {
        engine().tick(1.0 / 30.0);
    }
    engine().consumeEvents();

    autochess::GameSnapshot after = engine().snapshot();
    int afterSelfTower = towerHp(after, autochess::PlayerId::One);
    int afterEnemyTower = towerHp(after, autochess::PlayerId::Two);
    reward += (beforeEnemyTower - afterEnemyTower) / 1600.0;
    reward -= (beforeSelfTower - afterSelfTower) / 1600.0;
    if (after.winner) {
        reward += *after.winner == autochess::PlayerId::One ? 1.0 : -1.0;
    }

    ++g_stepCount;
    bool done = after.phase == autochess::Phase::Finished || g_stepCount >= g_maxSteps;
    PyObject* result = PyDict_New();
    PyObject* observation = snapshotDict();
    PyDict_SetItemString(result, "observation", observation);
    Py_DECREF(observation);
    setDouble(result, "reward", reward);
    setBool(result, "done", done);
    setBool(result, "applied", applied);
    PyObject* legal = legalActionsList();
    PyDict_SetItemString(result, "legalActions", legal);
    Py_DECREF(legal);
    return result;
}

PyObject* py_rules_fingerprint(PyObject*, PyObject*) {
    return PyUnicode_FromString(engine().rulesFingerprint().c_str());
}

PyObject* py_feature_schema(PyObject*, PyObject*) {
    autochess::AiFeatureSchema schema = engine().aiFeatureSchema();
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

PyObject* py_unit_catalog(PyObject*, PyObject*) {
    const std::vector<autochess::UnitSpec>& specs = engine().shop();
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
        setDouble(item, "speed", spec.speed);
        setString(item, "layer", autochess::toString(spec.layer));
        setBool(item, "canAttackAir", spec.canAttackAir);
        setInt(item, "threat", spec.threat);
        PyList_SET_ITEM(list, i, item);
    }
    return list;
}

PyMethodDef kMethods[] = {
    {"reset", reinterpret_cast<PyCFunction>(py_reset), METH_VARARGS | METH_KEYWORDS, "Reset the C++ AutoChess environment."},
    {"legal_actions", py_legal_actions, METH_NOARGS, "Return current legal preparation actions."},
    {"step", py_step, METH_VARARGS, "Apply a legal action by index."},
    {"rules_fingerprint", py_rules_fingerprint, METH_NOARGS, "Return the current rules fingerprint."},
    {"feature_schema", py_feature_schema, METH_NOARGS, "Return state/action feature schema metadata."},
    {"state_features", py_state_features, METH_NOARGS, "Return current state features."},
    {"action_features", py_action_features, METH_VARARGS, "Return features for a legal action index."},
    {"unit_catalog", py_unit_catalog, METH_NOARGS, "Return unit catalog exported from C++ rules."},
    {nullptr, nullptr, 0, nullptr}
};

PyModuleDef kModule = {
    PyModuleDef_HEAD_INIT,
    "autochess_env",
    "Headless AutoChess training environment backed by autochess_core.",
    -1,
    kMethods
};

} // namespace

PyMODINIT_FUNC PyInit_autochess_env() {
    return PyModule_Create(&kModule);
}
