// main.dart 

//--------- Imports and Global BLE Setup ---------
import 'dart:async'; //Handles streams
import 'dart:typed_data'; //Handles binary data
import 'package:flutter/material.dart'; //For Flutter UI
import 'package:flutter_reactive_ble/flutter_reactive_ble.dart'; //BLE Interaction
import 'dart:math'; //Waveform simulation


//--------- Define BLE Service UUIDs ---------//
final flutterReactiveBLE = FlutterReactiveBle(); //BLE Controller
final serviceUuid  = Uuid.parse("180D");
final hrCharUuid   = Uuid.parse("2A37");
final spo2CharUuid = Uuid.parse("2A5F");
final irCharUuid   = Uuid.parse("beb5483e-36e1-4688-b7f5-ea07361b26a8");

//---------Creates a BLE Manager class for scanning, connecting,and data streaming---------//
class BLEManager { //Uses a Singleton pattern to avoid multiple Bluetooth connections
  BLEManager._() { _init(); }  //Private constructor initializes the manager and prevents other parts of the app from creating additional instances
  static final BLEManager _instance = BLEManager._(); //Creates a single shared instance of the manager
  factory BLEManager() => _instance; //Returns the same BLEManager instance whenever it is requested

//--------- Creates a controller for each variable to broadcast incoming sensor data ---------//
  final _hrCtrl   = StreamController<int>.broadcast(); 
  final _spo2Ctrl = StreamController<int>.broadcast();
  final _irCtrl   = StreamController<int>.broadcast();

  //---------Provides read-only streams so UI components can listen for real-time updates---------//
  Stream<int> get hrStream   => _hrCtrl.stream;
  Stream<int> get spo2Stream => _spo2Ctrl.stream;
  Stream<int> get irStream   => _irCtrl.stream;

//---------Streaming and device listening---------//
  bool _started = false; //Flag that ensures Bluetooth setup only runs once
  StreamSubscription<DiscoveredDevice>? _scanSub; //Listens for nearby BLE devices while scanning
  StreamSubscription<ConnectionStateUpdate>? _connSub; //Listens for connection status changes
  StreamSubscription<List<int>>? _hrSub, _spo2Sub, _irSub; //Listens for incoming sensor data from the device

// FUNCTION Initialize bluetooth scanning///
  void _init() { 
    if (_started) return; //If bluetooth started, stop immediately
    _started = true; //Changes flag to on 
    flutterReactiveBLE.statusStream.listen((status) { //Gives bluetooth availability status
      if (status == BleStatus.ready) _startScan(); //Only start scanning when bluetooth is ready 
    });
  }

//FUNCTION Start bluetooth scanning for data acquisition//
  void _startScan() {
    _scanSub = flutterReactiveBLE //Starts BLE scan and stores inside _scanSub
      .scanForDevices(withServices: [serviceUuid]) //Only search for devices that provide a specific bluetooth service
      .listen((device) { //Everytime a device is discovered run this code
        if ((device.name ?? "").contains("Pulse")) { //Searches for devices containing the term "Pulse"
          _scanSub?.cancel(); //Stop scanning when device is found
          _connect(device.id); //Connects the device
        }
      }, onError: (e) => debugPrint("Scan error: $e"));
  }

//---------FUNCTION connect to device
  void _connect(String id) {
    _connSub = flutterReactiveBLE //Starts monitoring connection process and reports to _connSub
      .connectToDevice(id: id, connectionTimeout: const Duration(seconds: 5)) //Try connecting to device give up after 5s if fails
      .listen((update) { //Watch connection status continuously
        if (update.connectionState == DeviceConnectionState.connected) { //When device connects start streaming
          _subscribe(id); //Subscripe to data (UUIDs)
        } else if (update.connectionState == DeviceConnectionState.disconnected) { //If fail to connect cleanup and restart scanning
          _cleanupChars();
          _startScan();
        }
      }, onError: (e) {
        debugPrint("Connection error: $e");
        _startScan();
      });
  }

//---------FUNCTION subscribes to BLE sensor characteristics ---------//
  void _subscribe(String id) {
    //Creates characteristics//
    final hrChar = QualifiedCharacteristic(serviceId: serviceUuid, characteristicId: hrCharUuid, deviceId: id);
    final spo2Char = QualifiedCharacteristic(serviceId: serviceUuid, characteristicId: spo2CharUuid, deviceId: id);
    final irChar = QualifiedCharacteristic(serviceId: serviceUuid, characteristicId: irCharUuid, deviceId: id);

    _hrSub = flutterReactiveBLE.subscribeToCharacteristic(hrChar).listen((data) { //subscribe and listen to incoming HR data
      if (data.length >= 4) { //Checks if enough bytes were received to make a full number 
        final v = ByteData.sublistView(Uint8List.fromList(data)).getInt32(0, Endian.little); //Converts raw bytes to heart rate integer
        _hrCtrl.add(v); //Sends data to app streams
      }
    });

    _spo2Sub = flutterReactiveBLE.subscribeToCharacteristic(spo2Char).listen((data) {
      if (data.length >= 4) {
        final v = ByteData.sublistView(Uint8List.fromList(data)).getInt32(0, Endian.little);
        _spo2Ctrl.add(v);
      }
    });

    _irSub = flutterReactiveBLE.subscribeToCharacteristic(irChar).listen((data) {
      if (data.length >= 4) {
        final v = ByteData.sublistView(Uint8List.fromList(data)).getInt32(0, Endian.little);
        _irCtrl.add(v);
      }
    });
  }

//---------FUNCTION stops listening to sensor data and closes subscriptions---------//
  void _cleanupChars() {
    _hrSub?.cancel();
    _spo2Sub?.cancel();
    _irSub?.cancel();
  }

//---------FUNCTION Shuts down app when not in use ---------//
  void dispose() {
    _scanSub?.cancel();
    _connSub?.cancel();
    _cleanupChars();
    _hrCtrl.close();
    _spo2Ctrl.close();
    _irCtrl.close();
  }
}

//---------Starting point of application when launched --------//
void main() {
  WidgetsFlutterBinding.ensureInitialized(); //Intiializes flutter
  BLEManager(); //Retreives singleton BLE manager
  runApp(const PulseOximeterApp()); //Launches app 
}

//--------- Creates main flutter application ---------//
class PulseOximeterApp extends StatelessWidget { //Creates flutter widget called PulseOximeterApp
  const PulseOximeterApp({Key? key}) : super(key: key);
  @override
  Widget build(BuildContext context) {
    return MaterialApp( //UI foundation 
      title: 'Pulse Oximeter',
      theme: ThemeData(scaffoldBackgroundColor: Colors.white),
      home: const MainNavigator(),
      debugShowCheckedModeBanner: false,
    );
  }
}

//---------Creates main navigator widget ---------//
class MainNavigator extends StatefulWidget { //stateful as data and page  changes over time
  const MainNavigator({Key? key}) : super(key: key);
  @override State<MainNavigator> createState() => _MainNavigatorState(); //Connects widget to state controller
}

//--------MainNavigator Status--------//
class _MainNavigatorState extends State<MainNavigator> {
  int _index = 0; //Which page is displayed, metric is default
  final _pages = const [MetricsPage(), WaveformPage()]; //One page shows metrics and one page shows waveform


//---------Main UI Script---------//
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: _pages[_index], //displays whichever page corresponds to index number
      bottomNavigationBar: BottomNavigationBar( //creates navigation buttons at bottom of screen
        currentIndex: _index, //Which button should appear selected
        onTap: (i) => setState(() => _index = i), //Resets index 
        items: const [ //Defines nav buttons
          BottomNavigationBarItem(icon: Icon(Icons.bar_chart), label: 'Metrics'),
          BottomNavigationBarItem(icon: Icon(Icons.show_chart), label: 'Waveform'),
        ],
      ),
    );
  }
}


//----Real time data visualization ---------//
class MetricsPage extends StatelessWidget { //Creates screen called metrics page
  const MetricsPage({Key? key}) : super(key: key);
  @override
  Widget build(BuildContext context) {
    final ble = BLEManager(); 
    return Center( //centers screen
      child: Column( //ui vertical stack
        mainAxisAlignment: MainAxisAlignment.center, //centers vertically
        children: [
          StreamBuilder<int>( //listens to data and rebuilds UI when new data arrives
            stream: ble.irStream,
            builder: (c, s) => _box('IR Signal', s.data?.toString() ?? '-', '', Colors.purple), //Displays data if missing shows --
          ),
          const SizedBox(height: 20), //Adds padding in box
          StreamBuilder<int>(
            stream: ble.hrStream,
            builder: (c, s) => _box('Heart Rate', s.data?.toString() ?? '-', 'bpm', Colors.red),
          ),
          const SizedBox(height: 20),
          StreamBuilder<int>(
            stream: ble.spo2Stream,
            builder: (c, s) => _box('SpO₂', s.data?.toString() ?? '-', '%', Colors.blue),
          ),
        ],
      ),
    );
  }
}

//---------UI Helper Widget styling---------//
Widget _box(String label, String val, String unit, Color color) => Container( //widget label, value, and unit
  width: 280,
  padding: const EdgeInsets.symmetric(vertical: 16, horizontal: 24),
  decoration: BoxDecoration(color: color, borderRadius: BorderRadius.circular(16)),
  child: Column(
    children: [
      Text(label, style: const TextStyle(fontSize: 18, color: Colors.white)),
      const SizedBox(height: 8),
      Text('$val $unit', style: const TextStyle(fontSize: 32, color: Colors.white, fontWeight: FontWeight.bold)),
    ],
  ),
);

//---------Waveform screen setup ---------//
class WaveformPage extends StatelessWidget {
  const WaveformPage({Key? key}) : super(key: key);
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('IR Waveform')),
      body: Container(
        color: Colors.black,
        child: const WaveformView(), //loads widget responsible for waveform drawing
      ),
    );
  }
}


//--------_Widget that displays waveform--------//
//Note: The waveform display was implemented as a preliminary visualization. The primary focus of this project was the development of stable BLE communication and real-time metric monitoring.//
class WaveformView extends StatefulWidget {
  const WaveformView({Key? key}) : super(key: key);
  @override _WaveformViewState createState() => _WaveformViewState();
}

//Rolling buffer displaying 200 points at a time//
class _WaveformViewState extends State<WaveformView> {
  static const int maxSamples = 200;
  final List<int> buffer = List.filled(maxSamples, 0);
  late final StreamSubscription<int> sub;

@override


void initState() {
  super.initState();

  Timer.periodic(const Duration(milliseconds: 50), (timer) {
    final t = timer.tick;
    final v = (204000 + 5000 * sin(2 * pi * t / 50)).toInt(); // Simulate IR pulse

    setState(() {
      buffer.removeAt(0);
      buffer.add(v);
    });
  });
}


  @override
  void dispose() {
    sub.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return SizedBox.expand(
      child: CustomPaint(
        painter: WaveformPainter(buffer),
      ),
    );
  }
}


//--------Waveform engine ---------//
class WaveformPainter extends CustomPainter {
  final List<int> data;
  WaveformPainter(this.data);

  @override
  void paint(Canvas canvas, Size size) {
    if (data.isEmpty) return;

    final paint = Paint()
      ..color = Colors.greenAccent
      ..strokeWidth = 2
      ..style = PaintingStyle.stroke;
    final path = Path();

    // Rolling buffer keeping 200 points at a time
    const int window = 30;
    List<double> avgSubtracted = List.filled(data.length, 0);
    for (int i = 0; i < data.length; i++) {
      int start = i - window ~/ 2;
      int end = i + window ~/ 2;
      start = start.clamp(0, data.length - 1);
      end = end.clamp(0, data.length - 1);

      double sum = 0;
      for (int j = start; j <= end; j++) {
        sum += data[j];
      }
      double avg = sum / (end - start + 1);
      avgSubtracted[i] = data[i] - avg;
    }

    double minY = avgSubtracted.reduce((a, b) => a < b ? a : b);
    double maxY = avgSubtracted.reduce((a, b) => a > b ? a : b);
    double range = maxY - minY;
    double height = size.height;

    for (int i = 0; i < avgSubtracted.length; i++) {
      double x = i / (avgSubtracted.length - 1) * size.width;
      double y = range == 0 ? height / 2 : height - ((avgSubtracted[i] - minY) / range) * height;
      if (i == 0) path.moveTo(x, y);
      else path.lineTo(x, y);
    }

    canvas.drawPath(path, paint);
  }

  @override
  bool shouldRepaint(covariant WaveformPainter old) => true;
}
