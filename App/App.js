import React, { useState, useEffect, useRef } from "react";
import {
  View, Text, TextInput, TouchableOpacity, ScrollView,
  StyleSheet, Platform, Alert, StatusBar, Animated,
  Modal, FlatList, KeyboardAvoidingView, PermissionsAndroid,
} from "react-native";
import RNBluetoothClassic from "react-native-bluetooth-classic";

// ─── HELPERS ──────────────────────────────────────────────────────────────────
const pad    = (n) => String(n).padStart(2, "0");
const MONTHS = ["Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
const DAYS   = ["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];

const formatDateShort = (d) => `${d.getDate()} ${MONTHS[d.getMonth()]}`;
const formatDay       = (d) => DAYS[d.getDay()];

const buildPayload = (tasks, exam, habit, now = new Date()) => {
  const parts = [formatDateShort(now), formatDay(now)];
  tasks.forEach((t) => {
    parts.push(t.text.trim() || "");
    parts.push(t.time.trim() || "00:00");
  });
  parts.push(exam.name.trim() || "");
  parts.push(exam.date.trim() || "0 Jan");
  parts.push(habit.trim()     || "");
  return parts.join(",") + "\n";
};

// ─── APP ──────────────────────────────────────────────────────────────────────
export default function App() {
  const [device,      setDevice]      = useState(null);
  const [isConnected, setIsConnected] = useState(false);
  const [scanning,    setScanning]    = useState(false);
  const [pairedList,  setPairedList]  = useState([]);
  const [scanModal,   setScanModal]   = useState(false);

  // Pulse animation
  const pulse = useRef(new Animated.Value(1)).current;
  useEffect(() => {
    const loop = Animated.loop(
      Animated.sequence([
        Animated.timing(pulse, { toValue: 1.6, duration: 900, useNativeDriver: true }),
        Animated.timing(pulse, { toValue: 1,   duration: 900, useNativeDriver: true }),
      ])
    );
    isConnected ? loop.start() : loop.stop();
    return () => loop.stop();
  }, [isConnected]);

  // Live clock
  const [now, setNow] = useState(new Date());
  useEffect(() => {
    const t = setInterval(() => setNow(new Date()), 1000);
    return () => clearInterval(t);
  }, []);

  // Form state
  const [tasks, setTasks] = useState([{ text: "", time: "" }]);
  const [exam,  setExam]  = useState({ name: "", date: "" });
  const [habit, setHabit] = useState("");

  // ── Permissions ────────────────────────────────────────────────────────────
  const requestPermissions = async () => {
    if (Platform.OS !== "android") return true;
    if (Platform.Version >= 31) {
      const granted = await PermissionsAndroid.requestMultiple([
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
        PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
      ]);
      return (
        granted[PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN]    === "granted" &&
        granted[PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT] === "granted"
      );
    } else {
      const granted = await PermissionsAndroid.request(
        PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION
      );
      return granted === "granted";
    }
  };

  // ── Open paired-device modal ───────────────────────────────────────────────
  const openScanModal = async () => {
    const ok = await requestPermissions();
    if (!ok) { Alert.alert("Permission denied", "Bluetooth permission is required."); return; }
    setScanning(true);
    setScanModal(true);
    try {
      const bonded = await RNBluetoothClassic.getBondedDevices();
      setPairedList(bonded);
    } catch (e) {
      Alert.alert("Bluetooth error", e.message);
    } finally {
      setScanning(false);
    }
  };

  // ── Connect ────────────────────────────────────────────────────────────────
  const connectTo = async (d) => {
    setScanModal(false);
    try {
      const conn = await RNBluetoothClassic.connectToDevice(d.address);
      setDevice(conn);
      setIsConnected(true);
    } catch (e) {
      Alert.alert("Connection failed", e.message);
    }
  };

  // ── Disconnect ─────────────────────────────────────────────────────────────
  const disconnect = async () => {
    try {
      if (device) await RNBluetoothClassic.disconnectFromDevice(device.address);
    } catch (_) {}
    setDevice(null);
    setIsConnected(false);
  };

  // ── Send ───────────────────────────────────────────────────────────────────
  const sendData = async () => {
    if (!isConnected || !device) {
      Alert.alert("Not connected", "Please connect to HC-06 first.");
      return;
    }
    const payload = buildPayload(tasks, exam, habit, now);
    try {
      await RNBluetoothClassic.writeToDevice(device.address, payload);
      Alert.alert("Sent", payload.trim());
    } catch (e) {
      Alert.alert("Send error", e.message);
    }
  };

  // ── Task helpers ───────────────────────────────────────────────────────────
  const addTask    = () => setTasks((p) => [...p, { text: "", time: "" }]);
  const removeTask = (i) => setTasks((p) => p.filter((_, idx) => idx !== i));
  const updateText = (i, v) => setTasks((p) => p.map((t, idx) => idx === i ? { ...t, text: v } : t));
  const updateTime = (i, v) => setTasks((p) => p.map((t, idx) => idx === i ? { ...t, time: v } : t));

  // ─── RENDER ───────────────────────────────────────────────────────────────
  return (
    <KeyboardAvoidingView
      style={s.root}
      behavior={Platform.OS === "ios" ? "padding" : undefined}
    >
      <StatusBar barStyle="light-content" backgroundColor="#070d1c" />

      {/* ── CONNECTION BAR ── */}
      <View style={s.connBar}>
        <View style={s.connLeft}>
          <View style={s.dotWrap}>
            {isConnected && (
              <Animated.View style={[s.dotRing, { transform: [{ scale: pulse }] }]} />
            )}
            <View style={[s.dot, isConnected ? s.dotOn : s.dotOff]} />
          </View>
          <View>
            <Text style={s.connTitle}>ATmega32</Text>
            <Text style={[s.connSub, isConnected ? s.connSubOn : s.connSubOff]}>
              {isConnected ? `Connected  •  ${device?.name ?? "HC-06"}` : "Not connected"}
            </Text>
          </View>
        </View>
        <TouchableOpacity
          style={[s.connBtn, isConnected ? s.connBtnRed : s.connBtnBlue]}
          onPress={isConnected ? disconnect : openScanModal}
          activeOpacity={0.75}
        >
          <Text style={[s.connBtnTxt, isConnected ? s.connBtnTxtRed : s.connBtnTxtBlue]}>
            {isConnected ? "Disconnect" : "Connect BT"}
          </Text>
        </TouchableOpacity>
      </View>

      {/* ── SCROLL ── */}
      <ScrollView
        contentContainerStyle={s.scroll}
        showsVerticalScrollIndicator={false}
        keyboardShouldPersistTaps="handled"
      >
        <Text style={s.pageTitle}>Task Manager</Text>
        <Text style={s.pageSub}>TASKS  ·  EXAM  ·  HABIT</Text>

        {/* TASKS */}
        <Section title="Tasks">
          {tasks.map((t, i) => (
            <View key={i} style={s.taskBlock}>
              <TextInput
                style={s.input}
                placeholder={`Task ${i + 1} name`}
                placeholderTextColor="#2a3d58"
                value={t.text}
                onChangeText={(v) => updateText(i, v)}
              />
              <View style={s.labelRow}>
                <Text style={s.fieldLabel}>TIME</Text>
                {t.time !== "" && (
                  <TouchableOpacity onPress={() => updateTime(i, "")}>
                    <Text style={s.clearTxt}>clear</Text>
                  </TouchableOpacity>
                )}
              </View>
              <TextInput
                style={s.input}
                placeholder="e.g.  9:30  or  14:00"
                placeholderTextColor="#2a3d58"
                value={t.time}
                onChangeText={(v) => updateTime(i, v)}
              />
              {tasks.length > 1 && (
                <TouchableOpacity style={s.removeTaskBtn} onPress={() => removeTask(i)}>
                  <Text style={s.removeTaskTxt}>Remove task</Text>
                </TouchableOpacity>
              )}
              {i < tasks.length - 1 && <View style={s.divider} />}
            </View>
          ))}
          <TouchableOpacity style={s.addBtn} onPress={addTask} activeOpacity={0.7}>
            <Text style={s.addBtnTxt}>+ Add Task</Text>
          </TouchableOpacity>
        </Section>

        {/* EXAM */}
        <Section title="Exam">
          <TextInput
            style={s.input}
            placeholder="Exam name"
            placeholderTextColor="#2a3d58"
            value={exam.name}
            onChangeText={(v) => setExam((e) => ({ ...e, name: v }))}
          />
          <View style={[s.labelRow, { marginTop: 10 }]}>
            <Text style={s.fieldLabel}>DATE</Text>
            {exam.date !== "" && (
              <TouchableOpacity onPress={() => setExam((e) => ({ ...e, date: "" }))}>
                <Text style={s.clearTxt}>clear</Text>
              </TouchableOpacity>
            )}
          </View>
          <TextInput
            style={s.input}
            placeholder="e.g.  25 May  or  3 Jun"
            placeholderTextColor="#2a3d58"
            value={exam.date}
            onChangeText={(v) => setExam((e) => ({ ...e, date: v }))}
          />
        </Section>

        {/* HABIT */}
        <Section title="Habit">
          <TextInput
            style={s.input}
            placeholder="Habit to build"
            placeholderTextColor="#2a3d58"
            value={habit}
            onChangeText={setHabit}
          />
        </Section>

        {/* PAYLOAD PREVIEW */}
        <Section title="Payload Preview">
          <Text style={s.payload} selectable>
            {buildPayload(tasks, exam, habit, now)}
          </Text>
        </Section>

        {/* SEND */}
        <TouchableOpacity
          style={[s.sendBtn, !isConnected && s.sendBtnDim]}
          onPress={sendData}
          activeOpacity={0.8}
        >
          <Text style={s.sendTxt}>
            {isConnected ? "Send to ATmega32" : "Connect BT to Send"}
          </Text>
        </TouchableOpacity>

        <View style={{ height: 50 }} />
      </ScrollView>

      {/* ── PAIRED DEVICES MODAL ── */}
      <Modal visible={scanModal} transparent animationType="slide">
        <View style={s.overlay}>
          <View style={s.sheet}>
            <Text style={s.sheetTitle}>
              {scanning ? "Loading paired devices..." : "Paired Bluetooth Devices"}
            </Text>
            <Text style={s.sheetHint}>
              HC-06 must be paired in Android Settings first (PIN: 1234 or 0000)
            </Text>
            {pairedList.length === 0 && !scanning && (
              <Text style={s.noDevTxt}>
                No paired devices found.{"\n"}
                Go to Settings → Bluetooth → pair HC-06 first.
              </Text>
            )}
            <FlatList
              data={pairedList}
              keyExtractor={(item) => item.address}
              renderItem={({ item }) => (
                <TouchableOpacity style={s.deviceRow} onPress={() => connectTo(item)}>
                  <Text style={s.deviceName}>{item.name ?? "Unknown"}</Text>
                  <Text style={s.deviceId}>{item.address}</Text>
                </TouchableOpacity>
              )}
            />
            <TouchableOpacity
              style={s.cancelBtn}
              onPress={() => setScanModal(false)}
            >
              <Text style={s.cancelTxt}>Cancel</Text>
            </TouchableOpacity>
          </View>
        </View>
      </Modal>
    </KeyboardAvoidingView>
  );
}

// ─── SECTION COMPONENT ────────────────────────────────────────────────────────
function Section({ title, children }) {
  return (
    <View style={s.card}>
      <Text style={s.cardTitle}>{title.toUpperCase()}</Text>
      {children}
    </View>
  );
}

// ─── DESIGN TOKENS ────────────────────────────────────────────────────────────
const BG     = "#070d1c";
const SURF   = "#0d1526";
const SURF2  = "#111e33";
const BORDER = "#19293f";
const TEXT   = "#dde6f0";
const MUTED  = "#4d6480";
const GREEN  = "#10b981";
const RED    = "#ef4444";

const s = StyleSheet.create({
  root: { flex: 1, backgroundColor: BG },

  // Connection bar
  connBar: {
    flexDirection: "row", alignItems: "center", justifyContent: "space-between",
    backgroundColor: SURF, paddingHorizontal: 20, paddingTop: 54,
    paddingBottom: 16, borderBottomWidth: 1, borderBottomColor: BORDER,
  },
  connLeft:   { flexDirection: "row", alignItems: "center", gap: 12 },
  dotWrap:    { width: 18, height: 18, alignItems: "center", justifyContent: "center" },
  dot:        { width: 10, height: 10, borderRadius: 5, position: "absolute" },
  dotOn:      { backgroundColor: GREEN },
  dotOff:     { backgroundColor: RED },
  dotRing: {
    width: 20, height: 20, borderRadius: 10, borderWidth: 1.5,
    borderColor: GREEN, position: "absolute", opacity: 0.45,
  },
  connTitle:    { color: TEXT,  fontSize: 13, fontWeight: "700" },
  connSub:      { fontSize: 11, marginTop: 2 },
  connSubOn:    { color: GREEN },
  connSubOff:   { color: MUTED },
  connBtn:      { paddingHorizontal: 15, paddingVertical: 8, borderRadius: 8, borderWidth: 1 },
  connBtnBlue:    { backgroundColor: "rgba(59,130,246,0.12)", borderColor: "rgba(59,130,246,0.4)" },
  connBtnRed:     { backgroundColor: "rgba(239,68,68,0.1)",   borderColor: "rgba(239,68,68,0.35)" },
  connBtnTxt:     { fontSize: 12, fontWeight: "700" },
  connBtnTxtBlue: { color: "#93c5fd" },
  connBtnTxtRed:  { color: "#fca5a5" },

  // Scroll & header
  scroll:    { paddingHorizontal: 18, paddingTop: 30 },
  pageTitle: { color: TEXT,  fontSize: 30, fontWeight: "800", letterSpacing: -0.5 },
  pageSub:   { color: MUTED, fontSize: 11, marginTop: 5, marginBottom: 26, letterSpacing: 2.5 },

  // Card
  card: {
    backgroundColor: SURF, borderRadius: 14, padding: 16,
    marginBottom: 14, borderWidth: 1, borderColor: BORDER,
  },
  cardTitle: { color: MUTED, fontSize: 10, fontWeight: "700", letterSpacing: 2, marginBottom: 13 },

  // Task block
  taskBlock: { marginBottom: 10 },
  divider:   { height: 1, backgroundColor: BORDER, marginVertical: 10 },
  labelRow: {
    flexDirection: "row", justifyContent: "space-between",
    alignItems: "center", marginBottom: 6, marginTop: 8,
  },
  fieldLabel: { color: MUTED, fontSize: 10, fontWeight: "700", letterSpacing: 1.5 },
  clearTxt:   { color: RED,   fontSize: 11, fontWeight: "600" },

  // Input
  input: {
    backgroundColor: SURF2, borderRadius: 9, paddingHorizontal: 13,
    paddingVertical: 12, color: TEXT, fontSize: 14,
    borderWidth: 1, borderColor: BORDER,
  },

  // Remove task button
  removeTaskBtn: {
    marginTop: 8, alignSelf: "flex-end", paddingHorizontal: 10, paddingVertical: 5,
    borderRadius: 6, backgroundColor: "rgba(239,68,68,0.08)",
    borderWidth: 1, borderColor: "rgba(239,68,68,0.25)",
  },
  removeTaskTxt: { color: RED, fontSize: 11, fontWeight: "600" },

  // Add task button
  addBtn: {
    marginTop: 4, paddingVertical: 11, borderRadius: 9, borderWidth: 1,
    borderStyle: "dashed", borderColor: "rgba(59,130,246,0.3)", alignItems: "center",
  },
  addBtnTxt: { color: "#60a5fa", fontSize: 13, fontWeight: "600" },

  // Payload
  payload: {
    color: "#34d399", fontFamily: Platform.OS === "ios" ? "Courier New" : "monospace",
    fontSize: 11, lineHeight: 18, backgroundColor: "#020a14", padding: 11,
    borderRadius: 8, borderWidth: 1, borderColor: "rgba(52,211,153,0.12)",
  },

  // Send button
  sendBtn: {
    backgroundColor: GREEN, borderRadius: 13, paddingVertical: 17,
    alignItems: "center", marginTop: 4, shadowColor: GREEN,
    shadowOpacity: 0.3, shadowRadius: 16, shadowOffset: { width: 0, height: 4 }, elevation: 6,
  },
  sendBtnDim: { backgroundColor: SURF2, shadowOpacity: 0, elevation: 0 },
  sendTxt:    { color: "#fff", fontSize: 15, fontWeight: "800", letterSpacing: 0.3 },

  // Modal
  overlay: { flex: 1, backgroundColor: "rgba(0,0,0,0.78)", justifyContent: "flex-end" },
  sheet: {
    backgroundColor: SURF, borderTopLeftRadius: 22, borderTopRightRadius: 22,
    padding: 22, maxHeight: "65%", borderWidth: 1, borderBottomWidth: 0, borderColor: BORDER,
  },
  sheetTitle: { color: TEXT,  fontSize: 16, fontWeight: "700", textAlign: "center", marginBottom: 6 },
  sheetHint:  { color: MUTED, fontSize: 11, textAlign: "center", marginBottom: 16, lineHeight: 17 },
  noDevTxt:   { color: MUTED, textAlign: "center", marginVertical: 24, fontSize: 13, lineHeight: 20 },
  deviceRow: {
    backgroundColor: SURF2, padding: 14, borderRadius: 10,
    marginBottom: 8, borderWidth: 1, borderColor: BORDER,
  },
  deviceName: { color: TEXT,  fontWeight: "700", fontSize: 14 },
  deviceId:   { color: MUTED, fontSize: 11, marginTop: 3 },
  cancelBtn: {
    marginTop: 8, paddingVertical: 13, borderRadius: 11,
    borderWidth: 1, borderColor: "rgba(239,68,68,0.3)", alignItems: "center",
  },
  cancelTxt: { color: RED, fontWeight: "700", fontSize: 14 },
});
