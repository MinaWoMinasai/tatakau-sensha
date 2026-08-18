const SNAP_DEGREES = 15;
const POSITION_SNAP_DEGREES = 360 / 16;
const GAME_UNIT_TO_PIXEL = 80;
const DRAFT_STORAGE_KEY = "playerShipEditorDraftV1";

const PARAMETER_HELP = {
  classId: {
    name: "機体ID",
    jsonKey: "id",
    description: "進化ツリーやゲーム内部から機体を識別するための固有IDです。",
    value: "半角英数字とアンダースコアを使用します。既存IDを変更すると進化経路との対応が外れる場合があります。",
  },
  displayName: {
    name: "表示名",
    jsonKey: "displayName",
    description: "進化画面など、プレイヤー向けUIに表示する機体名です。",
    value: "見た目の名前だけを変更し、機体IDや性能には影響しません。",
  },
  bodyShape: {
    name: "機体形状",
    jsonKey: "bodyShape",
    description: "ネオン表示で使用する機体本体のシルエットを選びます。",
    value: "表示は日本語ですが、JSONには Circle / Box / Triangle / Pentagon の値を保存します。",
  },
  requiredRank: {
    name: "必要ランク",
    jsonKey: "requiredRank",
    description: "この機体へ進化するために必要なプレイヤーRankです。進化経路の直接edgeも別途必要です。",
    value: "値が大きいほど、より後のRankで選択可能になります。単位はRankです。",
  },
  reloadScale: {
    name: "リロード倍率",
    jsonKey: "reloadScale",
    description: "基本の射撃間隔に掛ける倍率です。ゲームでは基本間隔 × この値 × 砲塔個別倍率で計算します。",
    value: "1.0が基準です。小さいほど射撃間隔が短くなり連射が速く、大きいほど遅くなります。",
  },
  bulletSpeedScale: {
    name: "弾速倍率",
    jsonKey: "bulletSpeedScale",
    description: "プレイヤーの基本弾速に掛ける倍率です。通常弾の発射速度へ反映されます。",
    value: "1.0が基準です。大きいほど弾が速く、小さいほど遅くなります。",
  },
  bulletDamageScale: {
    name: "弾ダメージ倍率",
    jsonKey: "bulletDamageScale",
    description: "プレイヤーの基本攻撃ダメージに掛ける倍率です。各武器のダメージ計算の基準になります。",
    value: "1.0が基準です。2.0なら基準の2倍です。最終ダメージは最低1に補正されます。",
  },
  bulletCount: {
    name: "発射数",
    jsonKey: "bulletCount",
    description: "通常弾の砲塔が1回の攻撃で生成する弾数です。",
    value: "1が単発です。2以上では拡散角度の範囲内へ複数の弾を配置します。",
  },
  spreadAngleDeg: {
    name: "拡散角度",
    jsonKey: "spreadAngleDeg",
    description: "発射方向を中心として弾が広がる全体の角度幅です。",
    value: "単位は度（°）です。大きいほど広範囲、小さいほど正面へ集中します。0なら拡散しません。",
  },
  fireAllBarrels: {
    name: "全砲塔から発射",
    jsonKey: "fireAllBarrels",
    description: "ONにすると、発射可能なすべての砲塔を1回の射撃で同時に作動させます。",
    value: "ONの場合は「砲塔を交互発射」よりこちらが優先されます。",
  },
  alternateBarrels: {
    name: "砲塔を交互発射",
    jsonKey: "alternateBarrels",
    description: "ONにすると、複数砲塔または発射グループを射撃ごとに順番に切り替えます。",
    value: "「全砲塔から発射」がOFFのときに有効です。",
  },
  mountPreset: {
    name: "砲塔プリセット",
    jsonKey: "weaponMounts / fireAllBarrels / alternateBarrels",
    description: "よく使う砲塔数と配置をまとめて作成するエディター用機能です。",
    value: "適用すると現在の砲塔配列と発射方式をプリセット内容で置き換えます。",
  },
  weaponType: {
    name: "武器タイプ",
    jsonKey: "weaponMounts[].weaponType",
    description: "選択中の砲塔が実行する攻撃方式を指定します。",
    value: "表示は日本語ですが、JSONには Projectile / Laser / Mine / Melee の値を保存します。",
  },
  barrelShape: {
    name: "砲身形状",
    jsonKey: "weaponMounts[].barrelShape",
    description: "選択中の砲身をネオン表示するときの輪郭形状を選びます。",
    value: "主に見た目へ反映されます。JSONには Box / Heavy / Short / Wide / Trapezoid を保存します。",
  },
  moveMode: {
    name: "配置方法",
    jsonKey: "エディター専用（結果は weaponMounts[].offset）",
    description: "砲塔をドラッグするときの配置ルールです。外周固定は機体外周の16方向へ吸着し、自由移動は任意位置へ置けます。",
    value: "配置方法自体はJSONへ保存せず、決定した座標だけをoffsetへ保存します。",
  },
  autoAim: {
    name: "自動角度調整",
    jsonKey: "エディター専用（結果は weaponMounts[].angleDeg）",
    description: "ONにすると、砲塔を移動した方向に合わせて砲塔の向きも自動調整します。",
    value: "OFFでは砲塔角度を直接入力または回転操作で変更できます。",
  },
  offsetX: {
    name: "X座標",
    jsonKey: "weaponMounts[].offset[0]",
    description: "機体の向きを基準にした砲塔の前後位置です。",
    value: "正の値で前方、負の値で後方へ移動します。単位はゲーム内world unitです。",
  },
  offsetY: {
    name: "Y座標",
    jsonKey: "weaponMounts[].offset[1]",
    description: "機体の向きを基準にした砲塔の左右位置です。",
    value: "正の値で右側、負の値で左側へ移動します。単位はゲーム内world unitです。",
  },
  angleDeg: {
    name: "砲塔角度",
    jsonKey: "weaponMounts[].angleDeg",
    description: "機体の正面方向を0°とした、選択中砲塔の相対的な発射方向です。",
    value: "単位は度（°）です。エディターでは15°刻みで調整します。",
  },
};

const SAMPLE_ROOT = {
  version: 2,
  classes: [
    {
      id: "Prototype",
      displayName: "Prototype",
      bodyShape: "Triangle",
      bodyScale: [1, 1],
      bodyFillColor: [0.18, 0.28, 0.34, 0.38],
      bodyOutlineColor: [0.5, 1, 0.35, 1],
      weaponMounts: [
        {
          model: "gunBarrel.obj",
          barrelShape: "Box",
          offset: [0.72, 0, 0],
          scale: [1.25, 0.24, 0.24],
          angleDeg: 0,
          muzzleForward: 0.95,
          fires: true,
          weaponType: "Projectile",
        },
      ],
    },
  ],
};

const state = {
  rootJson: JSON.parse(JSON.stringify(SAMPLE_ROOT)),
  gameFileHandle: null,
  selectedClassIndex: 0,
  selectedMountIndex: 0,
  stage: null,
  layer: null,
  center: { x: 0, y: 0 },
  mountViews: [],
};

function isCoarsePointer() {
  return window.matchMedia?.("(pointer: coarse)").matches ?? false;
}

const stageContainer = document.getElementById("stage-container");
const canvasGuideToggle = document.getElementById("canvas-guide-toggle");
const canvasGuidePanel = document.getElementById("canvas-guide-panel");
const canvasModeSummary = document.getElementById("canvas-mode-summary");
const canvasSelectionHint = document.getElementById("canvas-selection-hint");
const fileInput = document.getElementById("json-file-input");
const classSelect = document.getElementById("class-select");
const newClassButton = document.getElementById("new-class-button");
const duplicateClassButton = document.getElementById("duplicate-class-button");
const mountSelect = document.getElementById("mount-select");
const bodyShapeSelect = document.getElementById("body-shape-select");
const moveModeSelect = document.getElementById("move-mode-select");
const autoAimCheckbox = document.getElementById("auto-aim-checkbox");
const addMountButton = document.getElementById("add-mount-button");
const duplicateMountButton = document.getElementById("duplicate-mount-button");
const deleteMountButton = document.getElementById("delete-mount-button");
const mountPresetSelect = document.getElementById("mount-preset-select");
const applyPresetButton = document.getElementById("apply-preset-button");
const weaponTypeSelect = document.getElementById("weapon-type-select");
const barrelShapeSelect = document.getElementById("barrel-shape-select");
const openGameJsonButton = document.getElementById("open-game-json-button");
const saveGameJsonButton = document.getElementById("save-game-json-button");
const exportButton = document.getElementById("export-json-button");
const gameJsonTarget = document.getElementById("game-json-target");
const directSaveSupport = document.getElementById("direct-save-support");
const statusText = document.getElementById("json-status");
const parameterHelpPanel = document.getElementById("parameter-help-panel");
const parameterHelpTitle = document.getElementById("parameter-help-title");
const parameterHelpJsonKey = document.getElementById("parameter-help-json-key");
const parameterHelpDescription = document.getElementById("parameter-help-description");
const parameterHelpValue = document.getElementById("parameter-help-value");
const parameterHelpClose = document.getElementById("parameter-help-close");
let activeHelpTrigger = null;
const classInputs = {
  id: document.getElementById("class-id-input"),
  name: document.getElementById("class-name-input"),
  requiredRank: document.getElementById("required-rank-input"),
  reloadScale: document.getElementById("reload-scale-input"),
  bulletSpeedScale: document.getElementById("bullet-speed-input"),
  bulletDamageScale: document.getElementById("bullet-damage-input"),
  bulletCount: document.getElementById("bullet-count-input"),
  spreadAngleDeg: document.getElementById("spread-angle-input"),
  fireAllBarrels: document.getElementById("fire-all-checkbox"),
  alternateBarrels: document.getElementById("alternate-checkbox"),
};
const inputs = {
  x: document.getElementById("mount-x"),
  y: document.getElementById("mount-y"),
  angle: document.getElementById("mount-angle"),
};

function normalizeAngleDeg(angleDeg) {
  let normalized = angleDeg % 360;
  if (normalized > 180) normalized -= 360;
  if (normalized <= -180) normalized += 360;
  return normalized;
}

function snapAngleDeg(angleDeg) {
  return normalizeAngleDeg(Math.round(angleDeg / SNAP_DEGREES) * SNAP_DEGREES);
}

function getClasses() {
  return Array.isArray(state.rootJson.classes) ? state.rootJson.classes : [];
}

function getSelectedClass() {
  return getClasses()[state.selectedClassIndex] ?? null;
}

function getMounts(playerClass) {
  if (!playerClass) return [];
  if (Array.isArray(playerClass.weaponMounts)) return playerClass.weaponMounts;
  if (Array.isArray(playerClass.barrels)) return playerClass.barrels;
  playerClass.weaponMounts = [];
  return playerClass.weaponMounts;
}

function cloneJson(value) {
  return JSON.parse(JSON.stringify(value));
}

function validatePlayerClassesRoot(rootJson) {
  if (!rootJson || typeof rootJson !== "object" || Array.isArray(rootJson)) {
    throw new Error("JSONルートがオブジェクトではありません。");
  }
  if (!Array.isArray(rootJson.classes)) {
    throw new Error("classes 配列が見つかりません。");
  }
  if (rootJson.classes.length === 0) {
    throw new Error("classes 配列が空です。");
  }

  rootJson.classes.forEach((playerClass, index) => {
    if (!playerClass || typeof playerClass !== "object" || Array.isArray(playerClass)) {
      throw new Error(`classes[${index}] がオブジェクトではありません。`);
    }
    if (typeof playerClass.id !== "string" || playerClass.id.trim().length === 0) {
      throw new Error(`classes[${index}].id が空です。`);
    }
  });
}

function applyLoadedJson(rootJson) {
  validatePlayerClassesRoot(rootJson);
  state.rootJson = rootJson;
  state.selectedClassIndex = 0;
  state.selectedMountIndex = 0;
  refreshClassSelect();
  renderEditor();
}

function updateDirectSaveUi() {
  const supported = typeof window.showOpenFilePicker === "function";
  openGameJsonButton.disabled = !supported;
  saveGameJsonButton.disabled = !supported || !state.gameFileHandle;
  gameJsonTarget.textContent = state.gameFileHandle?.name
    ?? "ゲーム用JSONが選択されていません";

  if (supported) {
    directSaveSupport.textContent = state.gameFileHandle
      ? "このファイルへ直接上書きできます"
      : "最初にゲーム用JSONを選択してください";
  } else {
    directSaveSupport.textContent =
      "このブラウザは直接保存に未対応です。JSON読み込みと別ファイル書き出しをご利用ください。";
  }
}

function setCanvasGuideVisible(visible) {
  canvasGuidePanel.hidden = !visible;
  canvasGuideToggle.setAttribute("aria-expanded", String(visible));
  canvasGuideToggle.textContent = visible ? "操作ガイドを隠す" : "操作ガイドを表示";
}

function updateCanvasGuideState() {
  const ringMode = moveModeSelect.value === "ring";
  canvasModeSummary.textContent = ringMode
    ? "外周固定：配置リング上の16方向へ吸着"
    : "自由移動：キャンバス内の任意位置へ配置";
  document.querySelectorAll("[data-ring-guide-only]").forEach((item) => {
    item.hidden = !ringMode;
  });
}

function initializeCanvasGuide() {
  setCanvasGuideVisible(window.innerWidth > 820);
  updateCanvasGuideState();
  canvasGuideToggle.addEventListener("click", () => {
    setCanvasGuideVisible(canvasGuidePanel.hidden);
  });
}

function closeParameterHelp(restoreFocus = true) {
  if (parameterHelpPanel.hidden) return;
  parameterHelpPanel.hidden = true;
  if (restoreFocus && activeHelpTrigger) {
    activeHelpTrigger.focus({ preventScroll: true });
  }
  activeHelpTrigger = null;
}

function openParameterHelp(helpKey, trigger) {
  const help = PARAMETER_HELP[helpKey];
  if (!help) return;

  parameterHelpTitle.textContent = help.name;
  parameterHelpJsonKey.textContent = help.jsonKey;
  parameterHelpDescription.textContent = help.description;
  parameterHelpValue.textContent = help.value;
  parameterHelpPanel.hidden = false;
  activeHelpTrigger = trigger;
  parameterHelpClose.focus({ preventScroll: true });
}

function initializeParameterHelp() {
  document.querySelectorAll("[data-help-key]").forEach((button) => {
    button.addEventListener("click", (event) => {
      event.preventDefault();
      event.stopPropagation();
      openParameterHelp(button.dataset.helpKey, button);
    });
    button.addEventListener("keydown", (event) => {
      if (event.key !== "Enter" && event.key !== " ") return;
      event.preventDefault();
      event.stopPropagation();
      openParameterHelp(button.dataset.helpKey, button);
    });
  });

  parameterHelpClose.addEventListener("click", () => closeParameterHelp());
  parameterHelpPanel.addEventListener("click", (event) => event.stopPropagation());
  document.addEventListener("click", () => closeParameterHelp(false));
  document.addEventListener("keydown", (event) => {
    if (event.key === "Escape" && !parameterHelpPanel.hidden) {
      event.preventDefault();
      closeParameterHelp();
    }
  });
}

function saveDraft(reason = "編集内容") {
  try {
    const draft = {
      savedAt: new Date().toISOString(),
      selectedClassIndex: state.selectedClassIndex,
      selectedMountIndex: state.selectedMountIndex,
      rootJson: state.rootJson,
    };
    localStorage.setItem(DRAFT_STORAGE_KEY, JSON.stringify(draft));
    statusText.textContent = `${reason}を端末内に下書き保存しました`;
  } catch (error) {
    statusText.textContent = `下書き保存に失敗しました: ${error.message}`;
  }
}

function loadDraft() {
  try {
    const text = localStorage.getItem(DRAFT_STORAGE_KEY);
    if (!text) return false;

    const draft = JSON.parse(text);
    if (!draft?.rootJson || !Array.isArray(draft.rootJson.classes)) {
      return false;
    }

    state.rootJson = draft.rootJson;
    state.selectedClassIndex = Math.min(
      Number(draft.selectedClassIndex) || 0,
      Math.max(0, getClasses().length - 1),
    );
    const mounts = getMounts(getSelectedClass());
    state.selectedMountIndex = Math.min(
      Number(draft.selectedMountIndex) || 0,
      Math.max(0, mounts.length - 1),
    );
    statusText.textContent = "端末内の下書きを復元しました";
    return true;
  } catch (error) {
    statusText.textContent = `下書き復元に失敗しました: ${error.message}`;
    return false;
  }
}

function makeUniqueClassId(baseId) {
  const safeBase = String(baseId || "CustomTank").replace(/[^A-Za-z0-9_]/g, "_") || "CustomTank";
  const existing = new Set(getClasses().map((playerClass) => playerClass.id));
  if (!existing.has(safeBase)) return safeBase;

  for (let i = 1; i < 1000; ++i) {
    const candidate = `${safeBase}_${i}`;
    if (!existing.has(candidate)) return candidate;
  }

  return `${safeBase}_${Date.now()}`;
}

function createDefaultMount(offset = [0.72, 0, 0], angleDeg = 0) {
  return {
    model: "gunBarrel.obj",
    barrelShape: "Box",
    offset,
    scale: [1.25, 0.24, 0.24],
    angleDeg,
    muzzleForward: 0.95,
    fires: true,
    weaponType: "Projectile",
    damageScale: 1.0,
    projectileSpeedScale: 1.0,
    fireGroup: 0,
    reloadScale: 1.0,
    recoilScale: 1.0,
    barrelColor: [0.25, 1.0, 0.95, 1.0],
    outlineColor: [0.8, 1.0, 0.95, 1.0],
    effectColor: [0.25, 1.0, 0.95, 1.0],
    laserRange: 18.0,
    laserWidth: 0.18,
    laserDuration: 0.12,
    laserDamageInterval: 0.08,
    mineRadius: 3.2,
    mineFuseTime: 0.45,
    mineLifeTime: 5.0,
    meleeRange: 3.4,
    meleeArcDeg: 105.0,
    meleeWidth: 0.2,
    meleeDuration: 0.18,
    meleeComboResetTime: 0.9,
    meleeCombo1DamageScale: 1.0,
    meleeCombo2DamageScale: 1.0,
    meleeCombo3DamageScale: 1.35,
    meleeCombo1RangeScale: 1.0,
    meleeCombo2RangeScale: 1.0,
    meleeCombo3RangeScale: 1.18,
    meleeCombo1Windup: 0.08,
    meleeCombo2Windup: 0.1,
    meleeCombo3Windup: 0.18,
    meleeCombo1Recovery: 0.1,
    meleeCombo2Recovery: 0.11,
    meleeCombo3Recovery: 0.24,
  };
}

function createDefaultClass(id) {
  return {
    id,
    displayName: id,
    requiredRank: 1,
    usesDrone: false,
    maxDrones: 7,
    reloadScale: 1.0,
    bulletSpeedScale: 1.0,
    bulletDamageScale: 1.0,
    bulletCount: 1,
    spreadAngleDeg: 10.0,
    randomSpread: true,
    reflect: false,
    penetrate: false,
    fireAllBarrels: false,
    alternateBarrels: false,
    recoilPower: 0.01,
    specialActionId: "perfect_dodge",
    specialActionCooldownScale: 1.0,
    specialActionStaminaCost: 1.0,
    saberCounterWindow: 0.28,
    saberCounterDamageScale: 2.5,
    saberCounterRangeScale: 1.35,
    bodyShape: "Circle",
    bodyScale: [1.0, 1.0],
    bodyFillColor: [0.18, 0.28, 0.34, 0.38],
    bodyOutlineColor: [0.5, 1.0, 0.35, 1.0],
    weaponMounts: [createDefaultMount()],
  };
}

function readVector3(value, fallback) {
  if (!Array.isArray(value)) return { ...fallback };
  return {
    x: Number(value[0] ?? fallback.x),
    y: Number(value[1] ?? fallback.y),
    z: Number(value[2] ?? fallback.z),
  };
}

function writeVector3(target, value) {
  target[0] = Number(value.x.toFixed(4));
  target[1] = Number(value.y.toFixed(4));
  target[2] = Number(value.z.toFixed(4));
}

function getOffsetRadius(offset) {
  const radius = Math.hypot(offset.x, offset.y);
  return radius > 0.001 ? radius : 0.72;
}

function constrainOffsetToRing(offset, radius) {
  const length = Math.hypot(offset.x, offset.y);
  if (length <= 0.001) {
    return { x: radius, y: 0, z: offset.z ?? 0 };
  }

  const scale = radius / length;
  return {
    x: offset.x * scale,
    y: offset.y * scale,
    z: offset.z ?? 0,
  };
}

function snapOffsetToDirections(offset, radius) {
  const rawAngleDeg = Math.atan2(offset.y, offset.x) * 180 / Math.PI;
  const snappedAngleDeg = Math.round(rawAngleDeg / POSITION_SNAP_DEGREES) * POSITION_SNAP_DEGREES;
  const rad = snappedAngleDeg * Math.PI / 180;
  return {
    x: Math.cos(rad) * radius,
    y: Math.sin(rad) * radius,
    z: offset.z ?? 0,
  };
}

function applyPlacementRules(offset, radius) {
  if (moveModeSelect.value !== "ring") {
    return offset;
  }

  const ringOffset = constrainOffsetToRing(offset, radius);
  return snapOffsetToDirections(ringOffset, radius);
}

function angleFromOffset(offset) {
  return snapAngleDeg(Math.atan2(offset.y, offset.x) * 180 / Math.PI);
}

function gameToWebPosition(offset, center) {
  // ゲーム側: offset.x が前方、offset.y が右方向。
  // Web画面では上方向を機体前方として表示する。
  return {
    x: center.x + offset.y * GAME_UNIT_TO_PIXEL,
    y: center.y - offset.x * GAME_UNIT_TO_PIXEL,
  };
}

function webToGameOffset(position, center) {
  return {
    x: (center.y - position.y) / GAME_UNIT_TO_PIXEL,
    y: (position.x - center.x) / GAME_UNIT_TO_PIXEL,
    z: 0,
  };
}

function gameAngleToWebRotation(angleDeg) {
  // Konvaは右向きが0度。ゲーム用0度を上向きで見せるため-90度ずらす。
  return normalizeAngleDeg(angleDeg - 90);
}

function webRotationToGameAngle(rotationDeg) {
  return normalizeAngleDeg(rotationDeg + 90);
}

function colorFromJson(value, fallback) {
  const source = Array.isArray(value) ? value : fallback;
  const r = Math.round((source[0] ?? fallback[0]) * 255);
  const g = Math.round((source[1] ?? fallback[1]) * 255);
  const b = Math.round((source[2] ?? fallback[2]) * 255);
  const a = source[3] ?? fallback[3];
  return `rgba(${r}, ${g}, ${b}, ${a})`;
}

function updateReadout() {
  const playerClass = getSelectedClass();
  const mounts = getMounts(playerClass);
  const mount = mounts[state.selectedMountIndex];

  if (!mount) {
    inputs.x.value = "";
    inputs.y.value = "";
    inputs.angle.value = "";
    inputs.x.disabled = true;
    inputs.y.disabled = true;
    inputs.angle.disabled = true;
    weaponTypeSelect.disabled = true;
    barrelShapeSelect.disabled = true;
    return;
  }

  const offset = readVector3(mount.offset, { x: 0, y: 0, z: 0 });
  inputs.x.disabled = false;
  inputs.y.disabled = false;
  inputs.angle.disabled = false;
  inputs.x.value = offset.x.toFixed(2);
  inputs.y.value = offset.y.toFixed(2);
  inputs.angle.value = Math.round(normalizeAngleDeg(Number(mount.angleDeg ?? 0))).toString();
  weaponTypeSelect.disabled = false;
  barrelShapeSelect.disabled = false;
  weaponTypeSelect.value = mount.weaponType ?? "Projectile";
  barrelShapeSelect.value = mount.barrelShape ?? "Box";
}

function getWeaponTypeLabel(weaponType) {
  return {
    Projectile: "通常弾",
    Laser: "レーザー",
    Mine: "地雷",
    Melee: "近接攻撃",
    Drone: "ドローン",
  }[weaponType] ?? "未設定";
}

function refreshMountSelect() {
  const mounts = getMounts(getSelectedClass());
  mountSelect.innerHTML = "";

  mounts.forEach((mount, index) => {
    const option = document.createElement("option");
    option.value = String(index);
    option.textContent = `${index}: ${getWeaponTypeLabel(mount.weaponType)}`;
    mountSelect.appendChild(option);
  });

  mountSelect.disabled = mounts.length === 0;
  addMountButton.disabled = !getSelectedClass();
  duplicateMountButton.disabled = mounts.length === 0;
  deleteMountButton.disabled = mounts.length <= 1;
  weaponTypeSelect.disabled = mounts.length === 0;
  barrelShapeSelect.disabled = mounts.length === 0;
  mountSelect.value = String(state.selectedMountIndex);
}

function updateSelectedMountFromInputs() {
  const mount = getMounts(getSelectedClass())[state.selectedMountIndex];
  const view = state.mountViews[state.selectedMountIndex];
  if (!mount || !view) return;

  const x = Number(inputs.x.value);
  const y = Number(inputs.y.value);
  const angle = Number(inputs.angle.value);
  if (!Number.isFinite(x) || !Number.isFinite(y) || !Number.isFinite(angle)) {
    return;
  }

  let offset = { x, y, z: 0 };
  offset = applyPlacementRules(offset, view.ringRadius);

  if (!Array.isArray(mount.offset)) mount.offset = [0, 0, 0];
  writeVector3(mount.offset, offset);
  mount.angleDeg = autoAimCheckbox.checked ? angleFromOffset(offset) : snapAngleDeg(angle);

  view.group.position(gameToWebPosition(offset, state.center));
  view.group.rotation(gameAngleToWebRotation(mount.angleDeg));
  updateReadout();
  renderGuideLayer();
  state.layer.batchDraw();
  saveDraft("砲塔");
}

function updateSelectedMountTypeFields() {
  const mount = getMounts(getSelectedClass())[state.selectedMountIndex];
  if (!mount) return;

  mount.weaponType = weaponTypeSelect.value;
  mount.barrelShape = barrelShapeSelect.value;
  refreshMountSelect();
  renderEditor();
  saveDraft("砲塔種類");
}

function refreshClassEditor() {
  const playerClass = getSelectedClass();
  const disabled = !playerClass;

  for (const input of Object.values(classInputs)) {
    input.disabled = disabled;
  }
  duplicateClassButton.disabled = disabled;

  if (!playerClass) {
    classInputs.id.value = "";
    classInputs.name.value = "";
    return;
  }

  classInputs.id.value = playerClass.id ?? "";
  classInputs.name.value = playerClass.displayName ?? "";
  classInputs.requiredRank.value = Number(playerClass.requiredRank ?? 1);
  classInputs.reloadScale.value = Number(playerClass.reloadScale ?? 1);
  classInputs.bulletSpeedScale.value = Number(playerClass.bulletSpeedScale ?? 1);
  classInputs.bulletDamageScale.value = Number(playerClass.bulletDamageScale ?? 1);
  classInputs.bulletCount.value = Number(playerClass.bulletCount ?? 1);
  classInputs.spreadAngleDeg.value = Number(playerClass.spreadAngleDeg ?? 0);
  classInputs.fireAllBarrels.checked = Boolean(playerClass.fireAllBarrels);
  classInputs.alternateBarrels.checked = Boolean(playerClass.alternateBarrels);
}

function updateSelectedClassFromInputs() {
  const playerClass = getSelectedClass();
  if (!playerClass) return;

  const oldId = playerClass.id;
  const nextId = String(classInputs.id.value || oldId || "CustomTank").replace(/[^A-Za-z0-9_]/g, "_");
  const duplicate = getClasses().some((item, index) => index !== state.selectedClassIndex && item.id === nextId);
  playerClass.id = duplicate ? oldId : nextId;
  classInputs.id.value = playerClass.id;
  playerClass.displayName = classInputs.name.value || playerClass.id;
  playerClass.requiredRank = Math.max(1, Math.round(Number(classInputs.requiredRank.value) || 1));
  playerClass.reloadScale = Math.max(0.05, Number(classInputs.reloadScale.value) || 1);
  playerClass.bulletSpeedScale = Math.max(0.05, Number(classInputs.bulletSpeedScale.value) || 1);
  playerClass.bulletDamageScale = Math.max(0.05, Number(classInputs.bulletDamageScale.value) || 1);
  playerClass.bulletCount = Math.max(1, Math.round(Number(classInputs.bulletCount.value) || 1));
  playerClass.spreadAngleDeg = Math.max(0, Number(classInputs.spreadAngleDeg.value) || 0);
  playerClass.fireAllBarrels = classInputs.fireAllBarrels.checked;
  playerClass.alternateBarrels = classInputs.alternateBarrels.checked;

  refreshClassSelect();
  refreshClassEditor();
  saveDraft("機体設定");
}

function updateSelectedClassBodyShape() {
  const playerClass = getSelectedClass();
  if (!playerClass) return;

  playerClass.bodyShape = bodyShapeSelect.value;
  renderEditor();
  saveDraft("機体形状");
}

function createNewClass() {
  const id = makeUniqueClassId("CustomTank");
  getClasses().push(createDefaultClass(id));
  state.selectedClassIndex = getClasses().length - 1;
  state.selectedMountIndex = 0;
  refreshClassSelect();
  renderEditor();
  statusText.textContent = `${id} を作成しました`;
  saveDraft("新規機体");
}

function duplicateSelectedClass() {
  const source = getSelectedClass();
  if (!source) return;

  const copy = cloneJson(source);
  copy.id = makeUniqueClassId(`${source.id || "CustomTank"}_Copy`);
  copy.displayName = `${source.displayName || source.id || "CustomTank"} のコピー`;
  getClasses().push(copy);
  state.selectedClassIndex = getClasses().length - 1;
  state.selectedMountIndex = 0;
  refreshClassSelect();
  renderEditor();
  statusText.textContent = `${copy.id} を複製作成しました`;
  saveDraft("機体複製");
}

function addMount() {
  const playerClass = getSelectedClass();
  if (!playerClass) return;

  const mounts = getMounts(playerClass);
  const source = mounts[state.selectedMountIndex] ? cloneJson(mounts[state.selectedMountIndex]) : createDefaultMount();
  const angle = mounts.length * POSITION_SNAP_DEGREES;
  const rad = angle * Math.PI / 180;
  const radius = getOffsetRadius(readVector3(source.offset, { x: 0.72, y: 0, z: 0 }));

  source.offset = [
    Number((Math.cos(rad) * radius).toFixed(4)),
    Number((Math.sin(rad) * radius).toFixed(4)),
    0,
  ];
  source.angleDeg = angleFromOffset(readVector3(source.offset, { x: 0.72, y: 0, z: 0 }));
  mounts.push(source);
  state.selectedMountIndex = mounts.length - 1;
  renderEditor();
  saveDraft("砲塔追加");
}

function duplicateMount() {
  const mounts = getMounts(getSelectedClass());
  const source = mounts[state.selectedMountIndex];
  if (!source) return;

  const copy = cloneJson(source);
  const offset = readVector3(copy.offset, { x: 0.72, y: 0, z: 0 });
  const radius = getOffsetRadius(offset);
  const angle = Math.atan2(offset.y, offset.x) * 180 / Math.PI + POSITION_SNAP_DEGREES;
  const rad = angle * Math.PI / 180;
  copy.offset = [
    Number((Math.cos(rad) * radius).toFixed(4)),
    Number((Math.sin(rad) * radius).toFixed(4)),
    0,
  ];
  copy.angleDeg = angleFromOffset(readVector3(copy.offset, { x: 0.72, y: 0, z: 0 }));
  mounts.splice(state.selectedMountIndex + 1, 0, copy);
  state.selectedMountIndex += 1;
  renderEditor();
  saveDraft("砲塔複製");
}

function deleteMount() {
  const mounts = getMounts(getSelectedClass());
  if (mounts.length <= 1) {
    statusText.textContent = "砲塔は最低1つ必要です";
    return;
  }

  mounts.splice(state.selectedMountIndex, 1);
  state.selectedMountIndex = Math.max(0, state.selectedMountIndex - 1);
  renderEditor();
  saveDraft("砲塔削除");
}

function makeRadialMount(angleDeg, radius = 0.72, scale = [1.25, 0.24, 0.24]) {
  const rad = angleDeg * Math.PI / 180;
  const mount = createDefaultMount([
    Number((Math.cos(rad) * radius).toFixed(4)),
    Number((Math.sin(rad) * radius).toFixed(4)),
    0,
  ], snapAngleDeg(angleDeg));
  mount.scale = scale;
  return mount;
}

function applyMountPreset() {
  const playerClass = getSelectedClass();
  if (!playerClass) return;

  const preset = mountPresetSelect.value;
  let mounts = [makeRadialMount(0)];
  playerClass.fireAllBarrels = false;
  playerClass.alternateBarrels = false;

  if (preset === "twin") {
    mounts = [makeRadialMount(-22.5), makeRadialMount(22.5)];
    playerClass.alternateBarrels = true;
  } else if (preset === "triple") {
    mounts = [makeRadialMount(-45), makeRadialMount(0), makeRadialMount(45)];
    playerClass.fireAllBarrels = true;
  } else if (preset === "quad") {
    mounts = [makeRadialMount(0), makeRadialMount(90), makeRadialMount(180), makeRadialMount(-90)];
    playerClass.fireAllBarrels = true;
  } else if (preset === "fan5") {
    mounts = [makeRadialMount(-45), makeRadialMount(-22.5), makeRadialMount(0), makeRadialMount(22.5), makeRadialMount(45)];
    playerClass.fireAllBarrels = true;
  }

  playerClass.weaponMounts = mounts;
  state.selectedMountIndex = 0;
  renderEditor();
  refreshClassEditor();
  saveDraft("プリセット");
}

function createGrid(width, height) {
  const grid = new Konva.Group({ listening: false });
  const step = 40;

  for (let x = 0; x <= width; x += step) {
    grid.add(new Konva.Line({
      points: [x, 0, x, height],
      stroke: x === width / 2 ? "rgba(125,241,255,0.16)" : "rgba(125,241,255,0.06)",
      strokeWidth: 1,
    }));
  }

  for (let y = 0; y <= height; y += step) {
    grid.add(new Konva.Line({
      points: [0, y, width, y],
      stroke: y === height / 2 ? "rgba(125,241,255,0.16)" : "rgba(125,241,255,0.06)",
      strokeWidth: 1,
    }));
  }

  state.layer.add(grid);
}

function createShip(playerClass) {
  const bodyShape = playerClass?.bodyShape ?? "Circle";
  const bodyScale = Array.isArray(playerClass?.bodyScale) ? playerClass.bodyScale : [1, 1];
  const fill = colorFromJson(playerClass?.bodyFillColor, [0.18, 0.28, 0.34, 0.92]);
  const stroke = colorFromJson(playerClass?.bodyOutlineColor, [0.5, 1, 0.35, 1]);
  const ship = new Konva.Group({ x: state.center.x, y: state.center.y, listening: false });

  if (bodyShape === "Circle") {
    ship.add(new Konva.Circle({
      radius: 52,
      scaleX: bodyScale[0] ?? 1,
      scaleY: bodyScale[1] ?? 1,
      fill,
      stroke,
      strokeWidth: 3,
      shadowColor: "#7dff9a",
      shadowBlur: 18,
      shadowOpacity: 0.55,
    }));
  } else {
    const sides = bodyShape === "Box" ? 4 : bodyShape === "Pentagon" ? 5 : 3;
    ship.add(new Konva.RegularPolygon({
      sides,
      radius: 58,
      rotation: bodyShape === "Box" ? 45 : 0,
      scaleX: bodyScale[0] ?? 1,
      scaleY: bodyScale[1] ?? 1,
      fill,
      stroke,
      strokeWidth: 3,
      shadowColor: "#7dff9a",
      shadowBlur: 18,
      shadowOpacity: 0.55,
    }));
  }

  ship.add(new Konva.Arrow({
    name: "front-guide",
    points: [-88, 10, -88, -80],
    stroke: "#7dff9a",
    fill: "#7dff9a",
    strokeWidth: 3,
    pointerLength: 10,
    pointerWidth: 10,
    shadowColor: "#7dff9a",
    shadowBlur: 10,
    shadowOpacity: 0.6,
  }));

  ship.add(new Konva.Text({
    name: "front-guide-label",
    x: -106,
    y: -108,
    width: 36,
    text: "前",
    align: "center",
    fill: "#7dff9a",
    fontSize: 15,
    fontStyle: "bold",
    fontFamily: "Arial, Yu Gothic, Meiryo",
    shadowColor: "#7dff9a",
    shadowBlur: 8,
    shadowOpacity: 0.45,
  }));

  state.layer.add(ship);
}

function createRingGuide(radius) {
  if (moveModeSelect.value !== "ring") {
    return;
  }

  state.layer.add(new Konva.Circle({
    name: "ring-guide",
    x: state.center.x,
    y: state.center.y,
    radius: radius * GAME_UNIT_TO_PIXEL,
    stroke: "rgba(220,235,242,0.36)",
    strokeWidth: 1.5,
    dash: [10, 7],
    listening: false,
  }));
}

function createMount(mountData, index) {
  const offset = readVector3(mountData.offset, { x: 0.72, y: 0, z: 0 });
  const ringRadius = getOffsetRadius(offset);
  const initialPosition = gameToWebPosition(offset, state.center);
  const mount = new Konva.Group({
    x: initialPosition.x,
    y: initialPosition.y,
    rotation: gameAngleToWebRotation(Number(mountData.angleDeg ?? 0)),
    draggable: true,
  });

  const scale = readVector3(mountData.scale, { x: 1.25, y: 0.24, z: 0.24 });
  const width = Math.max(38, scale.x * 52);
  const height = Math.max(14, scale.y * 70);
  const touchMode = isCoarsePointer();
  const selectionRadius = touchMode ? 58 : 46;
  const handleRadius = touchMode ? 18 : 11;
  const handleOffset = width + (touchMode ? 20 : 8);
  const fill = colorFromJson(mountData.barrelColor, [0.07, 0.15, 0.19, 1]);
  const stroke = colorFromJson(mountData.outlineColor, [0.37, 0.96, 1, 1]);

  const barrel = new Konva.Rect({
    x: -10,
    y: -height / 2,
    width,
    height,
    cornerRadius: mountData.barrelShape === "Heavy" ? 3 : 5,
    fill,
    stroke,
    strokeWidth: 3,
    shadowColor: "#5ef5ff",
    shadowBlur: 16,
    shadowOpacity: 0.6,
  });

  const core = new Konva.Circle({
    x: 0,
    y: 0,
    radius: 15,
    fill: "#101523",
    stroke: "#ff4fa3",
    strokeWidth: 3,
  });

  const selectionRing = new Konva.Circle({
    name: "selection-highlight",
    x: 0,
    y: 0,
    radius: selectionRadius,
    stroke: "#5ef5ff",
    strokeWidth: 2,
    dash: [10, 5],
    shadowColor: "#5ef5ff",
    shadowBlur: 12,
    shadowOpacity: 0.65,
    visible: false,
    listening: false,
  });

  const rotateConnector = new Konva.Line({
    name: "rotate-connector",
    points: [selectionRadius * 0.55, 0, handleOffset - handleRadius, 0],
    stroke: "rgba(255,79,163,0.72)",
    strokeWidth: 2,
    dash: [4, 4],
    visible: false,
    listening: false,
  });

  const rotateHandle = new Konva.Group({
    name: "rotate-handle",
    x: handleOffset,
    y: 0,
    visible: false,
  });

  rotateHandle.add(new Konva.Circle({
    radius: handleRadius,
    fill: "#17101c",
    stroke: "#ff4fa3",
    strokeWidth: 3,
    shadowColor: "#ff4fa3",
    shadowBlur: 14,
    shadowOpacity: 0.72,
  }));

  rotateHandle.add(new Konva.Text({
    x: -handleRadius,
    y: -handleRadius + (touchMode ? 2 : 0),
    width: handleRadius * 2,
    height: handleRadius * 2,
    text: "↻",
    align: "center",
    verticalAlign: "middle",
    fill: "#ffffff",
    fontSize: touchMode ? 22 : 15,
    fontStyle: "bold",
    fontFamily: "Arial, Yu Gothic, Meiryo",
    listening: false,
  }));

  mount.add(barrel, core, selectionRing, rotateConnector, rotateHandle);
  state.layer.add(mount);

  const view = { group: mount, selectionRing, rotateConnector, rotateHandle, ringRadius };
  state.mountViews[index] = view;

  mount.on("click tap", () => selectMount(index));

  mount.on("dragmove", () => {
    let nextOffset = webToGameOffset(mount.position(), state.center);
    nextOffset = applyPlacementRules(nextOffset, view.ringRadius);
    mount.position(gameToWebPosition(nextOffset, state.center));

    if (!Array.isArray(mountData.offset)) mountData.offset = [0, 0, 0];
    writeVector3(mountData.offset, nextOffset);
    if (autoAimCheckbox.checked) {
      mountData.angleDeg = angleFromOffset(nextOffset);
      mount.rotation(gameAngleToWebRotation(mountData.angleDeg));
    }
    updateReadout();
  });

  mount.on("dragend", () => {
    renderGuideLayer();
    state.layer.batchDraw();
    saveDraft("砲塔配置");
  });

  rotateHandle.on("mousedown touchstart", (event) => {
    event.cancelBubble = true;
    mount.draggable(false);
    const stage = mount.getStage();

    function rotateFromPointer() {
      const pointer = stage.getPointerPosition();
      if (!pointer) return;

      const dx = pointer.x - mount.x();
      const dy = pointer.y - mount.y();
      const rawWebRotation = Math.atan2(dy, dx) * 180 / Math.PI;
      const snappedGameAngle = snapAngleDeg(webRotationToGameAngle(rawWebRotation));
      mountData.angleDeg = snappedGameAngle;
      mount.rotation(gameAngleToWebRotation(snappedGameAngle));
      updateReadout();
    }

    function stopRotate() {
      mount.draggable(true);
      stage.off("mousemove touchmove", rotateFromPointer);
      stage.off("mouseup touchend", stopRotate);
      saveDraft("砲塔角度");
    }

    stage.on("mousemove touchmove", rotateFromPointer);
    stage.on("mouseup touchend", stopRotate);
    rotateFromPointer();
  });
}

function selectMount(index) {
  state.selectedMountIndex = index;

  state.mountViews.forEach((view, viewIndex) => {
    const selected = viewIndex === index;
    view.selectionRing.visible(selected);
    view.rotateConnector.visible(selected);
    view.rotateHandle.visible(selected);
    if (selected) view.group.moveToTop();
  });

  canvasSelectionHint.hidden = state.mountViews.length === 0;

  updateReadout();
  refreshMountSelect();
  renderGuideLayer();
  state.layer.batchDraw();
}

function renderGuideLayer() {
  state.layer.find(".ring-guide").forEach((node) => node.destroy());

  const view = state.mountViews[state.selectedMountIndex];
  if (!view || moveModeSelect.value !== "ring") {
    return;
  }

  const guideGroup = new Konva.Group({
    name: "ring-guide",
    listening: false,
  });
  const guideRadius = view.ringRadius * GAME_UNIT_TO_PIXEL;
  guideGroup.add(new Konva.Circle({
    x: state.center.x,
    y: state.center.y,
    radius: guideRadius,
    stroke: "rgba(220,235,242,0.40)",
    strokeWidth: 1.5,
    dash: [10, 7],
  }));

  const selectedMount = getMounts(getSelectedClass())[state.selectedMountIndex];
  const selectedOffset = readVector3(selectedMount?.offset, { x: 1, y: 0, z: 0 });
  const selectedAngleDeg = Math.atan2(selectedOffset.y, selectedOffset.x) * 180 / Math.PI;
  const selectedDirection = ((Math.round(selectedAngleDeg / POSITION_SNAP_DEGREES) % 16) + 16) % 16;

  for (let i = 0; i < 16; ++i) {
    const angle = i * POSITION_SNAP_DEGREES * Math.PI / 180;
    const offset = {
      x: Math.cos(angle) * view.ringRadius,
      y: Math.sin(angle) * view.ringRadius,
      z: 0,
    };
    const pos = gameToWebPosition(offset, state.center);
    const selected = i === selectedDirection;
    guideGroup.add(new Konva.Circle({
      x: pos.x,
      y: pos.y,
      radius: selected ? 5 : 3,
      fill: selected ? "#5ef5ff" : "rgba(220,235,242,0.52)",
      stroke: selected ? "#ffffff" : undefined,
      strokeWidth: selected ? 1 : 0,
      shadowColor: selected ? "#5ef5ff" : undefined,
      shadowBlur: selected ? 8 : 0,
      shadowOpacity: selected ? 0.65 : 0,
    }));
  }

  guideGroup.add(new Konva.Text({
    x: state.center.x + guideRadius + 8,
    y: state.center.y - 17,
    width: 108,
    text: "配置リング\n16方向候補",
    align: "left",
    fill: "rgba(220,235,242,0.72)",
    fontSize: 12,
    lineHeight: 1.35,
    fontFamily: "Arial, Yu Gothic, Meiryo",
  }));

  state.layer.add(guideGroup);
  guideGroup.zIndex(1);
}

function renderEditor() {
  state.layer.destroyChildren();
  state.mountViews = [];

  const playerClass = getSelectedClass();
  const mounts = getMounts(playerClass);

  createGrid(state.stage.width(), state.stage.height());
  createShip(playerClass);
  mounts.forEach((mount, index) => createMount(mount, index));

  state.selectedMountIndex = Math.min(state.selectedMountIndex, Math.max(0, mounts.length - 1));
  refreshBodyShapeSelect();
  refreshMountSelect();
  refreshClassEditor();
  if (mounts.length > 0) {
    selectMount(state.selectedMountIndex);
  } else {
    canvasSelectionHint.hidden = true;
    updateReadout();
  }

  state.layer.draw();
}

function refreshBodyShapeSelect() {
  const playerClass = getSelectedClass();
  bodyShapeSelect.disabled = !playerClass;
  bodyShapeSelect.value = playerClass?.bodyShape ?? "Circle";
}

function refreshClassSelect() {
  const classes = getClasses();
  classSelect.innerHTML = "";

  classes.forEach((playerClass, index) => {
    const option = document.createElement("option");
    option.value = String(index);
    option.textContent = playerClass.displayName || playerClass.id || `機体 ${index}`;
    classSelect.appendChild(option);
  });

  classSelect.disabled = classes.length === 0;
  classSelect.value = String(state.selectedClassIndex);
}

function loadJsonFile(file) {
  const reader = new FileReader();

  reader.onload = () => {
    try {
      const parsed = JSON.parse(String(reader.result));
      applyLoadedJson(parsed);
      state.gameFileHandle = null;
      updateDirectSaveUi();
      saveDraft(file.name);
      statusText.textContent = `${file.name} を読み込みました。直接保存先は未選択です。`;
    } catch (error) {
      statusText.textContent = `読み込み失敗: ${error.message}`;
    }
  };

  reader.onerror = () => {
    statusText.textContent = `${file.name} を読み込めませんでした。編集内容は維持されています。`;
  };

  reader.readAsText(file, "utf-8");
}

async function openGameJson() {
  if (typeof window.showOpenFilePicker !== "function") {
    updateDirectSaveUi();
    return;
  }

  try {
    const [fileHandle] = await window.showOpenFilePicker({
      multiple: false,
      types: [
        {
          description: "Player Classes JSON",
          accept: { "application/json": [".json"] },
        },
      ],
    });
    if (!fileHandle) return;

    const file = await fileHandle.getFile();
    const parsed = JSON.parse(await file.text());
    applyLoadedJson(parsed);
    state.gameFileHandle = fileHandle;
    updateDirectSaveUi();
    saveDraft(file.name);
    statusText.textContent = `${file.name} をゲーム用JSONとして読み込みました。`;
  } catch (error) {
    if (error?.name === "AbortError") {
      statusText.textContent = "ゲーム用JSONの選択をキャンセルしました。編集内容は維持されています。";
      return;
    }
    statusText.textContent = `ゲーム用JSONの読み込みに失敗しました: ${error.message}`;
  }
}

async function saveGameJson() {
  if (!state.gameFileHandle) {
    statusText.textContent = "先にゲーム用JSONを選択してください。";
    return;
  }

  let writable = null;
  try {
    validatePlayerClassesRoot(state.rootJson);
    const text = `${JSON.stringify(state.rootJson, null, 2)}\n`;
    writable = await state.gameFileHandle.createWritable();
    await writable.write(text);
    await writable.close();
    writable = null;
    saveDraft(state.gameFileHandle.name);
    statusText.textContent =
      `${state.gameFileHandle.name} へ保存しました。CG2は変更を自動再読み込みします。`;
  } catch (error) {
    if (writable) {
      try {
        await writable.abort();
      } catch {
        // 元の保存エラーを優先して表示する。
      }
    }
    if (error?.name === "AbortError" || error?.name === "NotAllowedError") {
      statusText.textContent =
        "ゲームへの保存をキャンセルしたか、書き込みが許可されませんでした。編集内容は維持されています。";
      return;
    }
    statusText.textContent = `ゲームへの保存に失敗しました: ${error.message}`;
  }
}

function exportJson() {
  const text = JSON.stringify(state.rootJson, null, 2);
  const blob = new Blob([text], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  const playerClass = getSelectedClass();
  const id = playerClass?.id || "playerClasses";

  link.href = url;
  link.download = `${id}_playerClasses_edited.json`;
  document.body.appendChild(link);
  link.click();
  link.remove();
  URL.revokeObjectURL(url);
  statusText.textContent = "編集済みJSONを書き出しました";
}

function initializeEditor() {
  if (!window.Konva) {
    stageContainer.textContent = "描画ライブラリを読み込めませんでした。ネット接続を確認してください。";
    stageContainer.classList.add("load-error");
    return;
  }

  const width = stageContainer.clientWidth;
  const height = stageContainer.clientHeight;
  state.center = { x: width / 2, y: height / 2 };

  state.stage = new Konva.Stage({
    container: "stage-container",
    width,
    height,
  });

  state.layer = new Konva.Layer();
  state.stage.add(state.layer);

  initializeCanvasGuide();
  initializeParameterHelp();

  stageContainer.addEventListener("touchmove", (event) => {
    event.preventDefault();
  }, { passive: false });

  fileInput.addEventListener("change", () => {
    const file = fileInput.files?.[0];
    if (file) loadJsonFile(file);
  });

  openGameJsonButton.addEventListener("click", openGameJson);
  saveGameJsonButton.addEventListener("click", saveGameJson);

  classSelect.addEventListener("change", () => {
    state.selectedClassIndex = Number(classSelect.value);
    state.selectedMountIndex = 0;
    renderEditor();
  });

  bodyShapeSelect.addEventListener("change", updateSelectedClassBodyShape);
  newClassButton.addEventListener("click", createNewClass);
  duplicateClassButton.addEventListener("click", duplicateSelectedClass);

  moveModeSelect.addEventListener("change", () => {
    updateCanvasGuideState();
    renderGuideLayer();
    state.layer.batchDraw();
  });

  autoAimCheckbox.addEventListener("change", updateSelectedMountFromInputs);

  mountSelect.addEventListener("change", () => {
    selectMount(Number(mountSelect.value));
  });

  addMountButton.addEventListener("click", addMount);
  duplicateMountButton.addEventListener("click", duplicateMount);
  deleteMountButton.addEventListener("click", deleteMount);
  applyPresetButton.addEventListener("click", applyMountPreset);
  weaponTypeSelect.addEventListener("change", updateSelectedMountTypeFields);
  barrelShapeSelect.addEventListener("change", updateSelectedMountTypeFields);

  inputs.x.addEventListener("change", updateSelectedMountFromInputs);
  inputs.y.addEventListener("change", updateSelectedMountFromInputs);
  inputs.angle.addEventListener("change", updateSelectedMountFromInputs);
  Object.values(classInputs).forEach((input) => {
    input.addEventListener("change", updateSelectedClassFromInputs);
  });

  exportButton.addEventListener("click", exportJson);

  let resizeTimer = 0;
  function scheduleResize() {
    window.clearTimeout(resizeTimer);
    resizeTimer = window.setTimeout(() => {
      if (!state.stage) return;
      const width = stageContainer.clientWidth;
      const height = stageContainer.clientHeight;
      if (width <= 0 || height <= 0) return;
      if (state.stage.width() === width && state.stage.height() === height) return;

      state.center = { x: width / 2, y: height / 2 };
      state.stage.size({ width, height });
      renderEditor();
    }, 120);
  }

  window.addEventListener("resize", scheduleResize);
  window.addEventListener("orientationchange", scheduleResize);

  updateDirectSaveUi();
  loadDraft();
  refreshClassSelect();
  renderEditor();
}

initializeEditor();
