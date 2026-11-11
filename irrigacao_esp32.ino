#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <time.h>       


const char* ssid = "iPhone (2)";
const char* password = "analuisalinda";


#define SENSOR_PIN 34  
#define RELE_PIN 14    


const int SENSOR_SECO = 4095;    
const int SENSOR_MOLHADO = 1800; 

const char* NTP_SERVIDOR_1 = "a.st1.ntp.br";
const char* NTP_SERVIDOR_2 = "pool.ntp.org";
const long  FUSO_HORARIO_SEG = -3 * 3600;

const char* LOG_ARQUIVO = "/log.txt";          
const char* LOG_ARQUIVO_ANTIGO = "/log_antigo.txt"; 
const size_t LOG_TAMANHO_MAX = 16 * 1024;      





WebServer server(80);
float umidadeMinima = 40.0; 


int umidadeAtual = 0;
float umidadeMediaAtual = 0.0;
int releEstadoAtual = 0; 


unsigned long previousMillis = 0;
const long intervalo = 5000; 

const int N_LEITURAS = 10;
float umidBuffer[N_LEITURAS] = {0};
int bufferIndex = 0;
int totalLeituras = 0;


bool fsOk = false;               
bool wifiConectado = false;      
bool servidorIniciado = false;   
bool horaSincronizada = false;   
unsigned long ultimaVerificacaoWifi = 0;
const long intervaloWifi = 10000; 
int falhasReconexao = 0;          


const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-br">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Controle de Irrigação</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Roboto:wght@400;700&display=swap');
    * { margin: 0; padding: 0; box-sizing: border-box; user-select: none; }
    body {
      font-family: 'Roboto', sans-serif;
      background: linear-gradient(135deg, #001f21, #003133, #001c1d);
      background-size: 400% 400%;
      animation: gradientBG 20s ease infinite;
      color: #e4e4e4;
      min-height: 100vh;
      display: flex; flex-direction: column; justify-content: center; align-items: center;
      padding: 20px; text-align: center;
    }
    @keyframes gradientBG { 0%{background-position:0% 50%} 50%{background-position:100% 50%} 100%{background-position:0% 50%} }
    @keyframes fadeInUp { from { opacity: 0; transform: translateY(30px); } to { opacity: 1; transform: translateY(0); } }
    h1 { font-size: clamp(2rem, 6vw, 2.8rem); margin-bottom: 10px; color: #ffffff; text-shadow: 0 0 10px rgba(255,255,255,0.2); letter-spacing: 2px; animation: fadeInUp 0.6s ease-out both; }
    .gauges { display: flex; justify-content: center; align-items: flex-start; max-width: 600px; width: 100%; margin-top: 30px; }
    .gauge { position: relative; width: 180px; height: 180px; user-select: none; display: flex; flex-direction: column; align-items: center; }
    .gauge svg { width: 180px; height: 180px; border-radius: 50%; transform: rotate(-90deg); box-shadow: 0 0 15px #0b9dac88, inset 0 0 40px #0b9dac55; background: radial-gradient(circle at center, #0b9dac22, transparent 70%); z-index: 0; overflow: visible; }
    .circle-bg { fill: none; stroke: #084e55; stroke-width: 4; }
    .circle { fill: none; stroke: #0b9dac; stroke-width: 4; stroke-linecap: round; transition: stroke-dasharray 0.6s ease; z-index: 1; }
    .value { position: absolute; top: 50%; left: 50%; transform: translate(-50%, -50%); font-size: 2rem; font-weight: 700; color: #0b9dac; display: flex; align-items: baseline; gap: 3px; user-select: none; line-height: 1; z-index: 2; pointer-events: none; }
    .unit { font-size: 0.9rem; font-weight: 500; color: #0b9dacbb; user-select: none; }
    .label { margin-top: 15px; font-size: 1.3rem; font-weight: 700; color: #e4e4e4; letter-spacing: 1px; user-select: none; z-index: 3; }
    .medias { margin-top: 40px; display: flex; gap: 60px; justify-content: center; flex-wrap: wrap; }
    .media-item { font-size: 1.4rem; font-weight: 700; user-select: none; display: flex; align-items: baseline; gap: 6px; }
    .media-item .label { color: #0B9DAC; font-weight: 700; }
    .media-item .valor { color: white; font-weight: 900; }
    .media-item .unit { color: white; font-weight: 500; font-size: 1.1rem; }
    .controles { background-color: rgba(0, 0, 0, 0.2); padding: 20px; border-radius: 12px; margin-top: 20px; display: flex; flex-direction: column; gap: 15px; align-items: center; min-width: 300px; animation: fadeInUp 0.8s ease-out 0.2s both; }
    .controle-item { display: flex; align-items: center; gap: 10px; font-size: 1.1rem;}
    .controle-item label { color: #0B9DAC; font-weight: 700; }
    .controle-item input { font-family: 'Roboto', sans-serif; font-size: 1.1rem; padding: 8px; width: 80px; background-color: #003133; border: 1px solid #084e55; color: white; border-radius: 5px; text-align: center; }
    .controle-item button { font-family: 'Roboto', sans-serif; font-size: 1rem; padding: 8px 15px; background-color: #0b9dac; color: white; border: none; border-radius: 5px; cursor: pointer; transition: background-color 0.3s ease; }
    .controle-item button:hover { background-color: #087a85; }
    .status-rele { font-weight: 700; }
    #statusRele.ligado { color: #23d18b; text-shadow: 0 0 8px #23d18b88; }
    #statusRele.desligado { color: #ff5252; text-shadow: 0 0 8px #ff525288; }
    #confirmMsg { color: #23d18b; font-weight: 700; margin-top: 10px; opacity: 0; transition: opacity 0.5s ease; user-select: none; height: 1.2em; }
    #confirmMsg.show { opacity: 1; }
    .info-rede { font-size: 1rem; }
    .info-rede span.valor-info { color: white; font-weight: 700; }
    #statusConexao.conectado { color: #23d18b; text-shadow: 0 0 8px #23d18b88; font-weight: 700; }
    #statusConexao.desconectado { color: #ff5252; text-shadow: 0 0 8px #ff525288; font-weight: 700; }
    .log-box { width: 100%; max-width: 600px; max-height: 260px; overflow-y: auto; text-align: left; font-family: monospace; font-size: 0.8rem; background-color: #00191a; border: 1px solid #084e55; border-radius: 8px; padding: 10px; user-select: text; }
    .log-box div { padding: 2px 0; border-bottom: 1px solid #08343a; white-space: pre-wrap; word-break: break-word; user-select: text; }
    .log-erro { color: #ff5252; } .log-aviso { color: #ffc857; } .log-info { color: #9fe8ef; }
    footer { margin-top: 40px; font-size: 0.9rem; color: #9ca3af; opacity: 0.7; animation: fadeInUp 0.8s ease-out 0.6s both; }
    @media (max-width: 600px) {
        .gauges { flex-wrap: wrap; gap: 40px; } .gauge, .gauge svg { width: 140px; height: 140px; }
        .value { font-size: 1.6rem; } .unit { font-size: 0.8rem; } .label { font-size: 1.1rem; margin-top: 10px; }
        .medias { gap: 30px; } .media-item { font-size: 1.2rem; } .media-item .unit { font-size: 1rem; } .controles { min-width: 90%; }
    }
  </style>
</head>
<body>
  <h1>Controle de Irrigação</h1>
  <div class="gauges">
    <div class="gauge">
      <svg viewBox="0 0 36 36"><path class="circle-bg" d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"/><path class="circle" id="circleUmid" stroke-dasharray="0, 100" d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"/></svg>
      <div class="value" id="umidValue">--<span class="unit">%</span></div><div class="label">Umidade do Solo</div>
    </div>
  </div>
  <div class="medias">
    <div class="media-item"><span class="label">Umidade Média = </span><span class="valor" id="umidMediaValue">--</span><span class="unit">%</span></div>
  </div>
  <div class="controles">
    <div class="controle-item">
      <label for="umidMinInput">Umid. Mínima (%):</label>
      <input type="number" id="umidMinInput" step="1" value="40">
      <button onclick="definirUmidadeMin(event)">Definir</button>
    </div>
    <div id="confirmMsg">Umidade mínima definida com sucesso!</div>
    <div class="controle-item">
      <label>Status da Irrigação:</label>
      <span id="statusRele" class="status-rele desligado">PARADA</span>
    </div>
  </div>
  <!-- [NOVO] Informações de rede do dispositivo -->
  <div class="controles info-rede">
    <div class="controle-item"><label>Status da Conexão:</label><span id="statusConexao" class="desconectado">--</span></div>
    <div class="controle-item"><label>Rede (SSID):</label><span class="valor-info" id="ssidValue">--</span></div>
    <div class="controle-item"><label>Sinal (RSSI):</label><span class="valor-info" id="rssiValue">--</span></div>
    <div class="controle-item"><label>Endereço IP:</label><span class="valor-info" id="ipValue">--</span></div>
    <div class="controle-item"><label>MAC Address:</label><span class="valor-info" id="macValue">--</span></div>
    <div class="controle-item"><label>Data/Hora do ESP32:</label><span class="valor-info" id="dataHoraValue">--</span></div>
  </div>
  <!-- [NOVO] Log do sistema (persistente na flash) -->
  <div class="controles">
    <div class="controle-item"><label>Log do Sistema</label><button onclick="atualizarLog()">Atualizar</button></div>
    <div class="log-box" id="logBox">Carregando...</div>
  </div>
  <footer>Eletrônica Ômega | E-book IoT com ESP32</footer>
  <script>
    async function definirUmidadeMin(event) {
      const botao = event.target;
      botao.disabled = true;
      const umidMin = document.getElementById('umidMinInput').value;
      if (umidMin === '' || isNaN(parseFloat(umidMin))) {
        alert('Por favor, insira um valor numérico válido.');
        botao.disabled = false; return;
      }
      try {
        const res = await fetch(`/set?umidade=${umidMin}`);
        if (!res.ok) throw new Error('Erro no servidor');
      } catch (error) {
        alert('Erro ao definir umidade no servidor.');
        botao.disabled = false; return;
      }
      mostrarConfirmacao();
      botao.disabled = false;
    }
    function mostrarConfirmacao() {
      const msg = document.getElementById('confirmMsg');
      msg.classList.add('show');
      setTimeout(() => { msg.classList.remove('show'); }, 2500);
    }
    async function atualizarDados() {
      try {
        const res = await fetch('/dados');
        const data = await res.json();
        const umid = parseFloat(data.umidade);
        document.getElementById('umidValue').innerHTML = umid.toFixed(0) + '<span class="unit">%</span>';
        const percentUmid = Math.min(umid, 100);
        document.getElementById('circleUmid').setAttribute('stroke-dasharray', `${percentUmid}, 100`);
        const umidMedia = parseFloat(data.umidMedia);
        document.getElementById('umidMediaValue').textContent = isNaN(umidMedia) ? '--' : umidMedia.toFixed(0);
        const umidMin = parseFloat(data.umidadeMinima);
        const inputUmidMin = document.getElementById('umidMinInput');
        if(document.activeElement !== inputUmidMin) { inputUmidMin.value = umidMin.toFixed(0); }
        const statusReleEl = document.getElementById('statusRele');
        if (data.releEstado === 1) {
          statusReleEl.textContent = 'IRRIGANDO';
          statusReleEl.className = 'status-rele ligado';
        } else {
          statusReleEl.textContent = 'PARADA';
          statusReleEl.className = 'status-rele desligado';
        }
        // [NOVO] Dados de rede
        const statusConexaoEl = document.getElementById('statusConexao');
        statusConexaoEl.textContent = data.conectado ? 'CONECTADO' : 'DESCONECTADO';
        statusConexaoEl.className = data.conectado ? 'conectado' : 'desconectado';
        document.getElementById('ssidValue').textContent = data.ssid;
        document.getElementById('rssiValue').textContent = data.rssi + ' dBm';
        document.getElementById('ipValue').textContent = data.ip;
        document.getElementById('macValue').textContent = data.mac;
        document.getElementById('dataHoraValue').textContent = data.dataHora;
      } catch (error) {
        console.error('Erro ao atualizar dados:', error);
        // [NOVO] Sem resposta do servidor = sem conexão com o ESP32
        const statusConexaoEl = document.getElementById('statusConexao');
        statusConexaoEl.textContent = 'SEM CONEXÃO COM O ESP32';
        statusConexaoEl.className = 'desconectado';
      }
    }
    // [NOVO] Busca o log persistente no ESP32 e mostra do mais recente para o mais antigo
    async function atualizarLog() {
      try {
        const res = await fetch('/log');
        const texto = await res.text();
        const caixa = document.getElementById('logBox');
        caixa.innerHTML = '';
        const linhas = texto.split('\n').filter(l => l.trim() !== '').reverse();
        if (linhas.length === 0) { caixa.textContent = 'Log vazio.'; return; }
        linhas.forEach(l => {
          const div = document.createElement('div');
          div.textContent = l;
          if (l.includes('[ERRO]')) div.className = 'log-erro';
          else if (l.includes('[AVISO]')) div.className = 'log-aviso';
          else div.className = 'log-info';
          caixa.appendChild(div);
        });
      } catch (error) { console.error('Erro ao carregar log:', error); }
    }
    setInterval(atualizarDados, 2000);
    setInterval(atualizarLog, 10000); // [NOVO] Atualiza o log a cada 10 s
    window.onload = () => { atualizarDados(); atualizarLog(); };
  </script>
</body>
</html>
)rawliteral";



String dataHoraAtual() {
  if (!horaSincronizada) {
    return "sem hora NTP, " + String(millis() / 1000) + "s apos boot";
  }
  time_t agora = time(nullptr);
  struct tm infoTempo;
  localtime_r(&agora, &infoTempo);
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", &infoTempo);
  return String(buffer);
}



void registrarLog(const char* nivel, const String& mensagem) {
  String linha = "[" + dataHoraAtual() + "] [" + nivel + "] " + mensagem;
  Serial.println(linha);
  if (!fsOk) return;


  File atual = LittleFS.open(LOG_ARQUIVO, "r");
  if (atual) {
    size_t tamanho = atual.size();
    atual.close();
    if (tamanho > LOG_TAMANHO_MAX) {
      LittleFS.remove(LOG_ARQUIVO_ANTIGO);
      LittleFS.rename(LOG_ARQUIVO, LOG_ARQUIVO_ANTIGO);
    }
  }

  File arquivo = LittleFS.open(LOG_ARQUIVO, FILE_APPEND);
  if (arquivo) {
    arquivo.println(linha);
    arquivo.close();
  }
}

// =================================================================
// --- FUNÇÕES DO SERVIDOR WEB ---
// =================================================================

// Função que serve a página HTML principal
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleDados() {
  String json = "{";
  json += "\"umidade\":" + String(umidadeAtual);
  json += ",\"umidMedia\":" + String(umidadeMediaAtual, 1);
  json += ",\"umidadeMinima\":" + String(umidadeMinima, 1);
  json += ",\"releEstado\":" + String(releEstadoAtual);
  json += ",\"conectado\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false");
  json += ",\"ssid\":\"" + WiFi.SSID() + "\"";
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"ip\":\"" + WiFi.localIP().toString() + "\"";
  json += ",\"mac\":\"" + WiFi.macAddress() + "\"";
  json += ",\"dataHora\":\"" + dataHoraAtual() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetUmidade() {
  if (server.hasArg("umidade")) {
    umidadeMinima = server.arg("umidade").toFloat();
    server.send(200, "text/plain", "OK");
    registrarLog("INFO", "Umidade minima alterada pela web para " + String(umidadeMinima, 0) + "% (cliente " + server.client().remoteIP().toString() + ")");
  } else {
    server.send(400, "text/plain", "Argumento 'umidade' ausente");
    registrarLog("AVISO", "Requisicao /set recebida sem o argumento 'umidade'");
  }
}

void handleLog() {
  if (fsOk && LittleFS.exists(LOG_ARQUIVO)) {
    File arquivo = LittleFS.open(LOG_ARQUIVO, "r");
    server.streamFile(arquivo, "text/plain; charset=utf-8");
    arquivo.close();
  } else {
    server.send(200, "text/plain; charset=utf-8", "");
  }
}


void iniciarServicosDeRede() {
  configTime(FUSO_HORARIO_SEG, 0, NTP_SERVIDOR_1, NTP_SERVIDOR_2);
  if (!servidorIniciado) {
    server.on("/", handleRoot);
    server.on("/dados", handleDados);
    server.on("/set", HTTP_GET, handleSetUmidade);
    server.on("/log", handleLog);
    server.begin();
    servidorIniciado = true;
    registrarLog("INFO", "Servidor web iniciado na porta 80");
  }
}


void verificarWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConectado) {
      wifiConectado = true;
      falhasReconexao = 0;
      registrarLog("INFO", "Wi-Fi reconectado. IP: " + WiFi.localIP().toString() + " | RSSI: " + String(WiFi.RSSI()) + " dBm");
      iniciarServicosDeRede();
    }
  } else {
    if (wifiConectado) {
      wifiConectado = false;
      registrarLog("AVISO", "Conexao Wi-Fi perdida. Tentando reconectar...");
    }
    falhasReconexao++;
    WiFi.disconnect();
    WiFi.begin(ssid, password);
    if (falhasReconexao == 6) { // 6 tentativas x 10 s = 1 minuto sem rede
      registrarLog("ERRO", "Sem Wi-Fi ha 1 minuto. Sistema segue em modo autonomo e continua tentando.");
    }
  }
}


void logicaDeIrrigacao() {
  // 1. Lê o valor do sensor
  int valorAnalogico = analogRead(SENSOR_PIN);
  umidadeAtual = map(valorAnalogico, SENSOR_SECO, SENSOR_MOLHADO, 0, 100);
  umidadeAtual = constrain(umidadeAtual, 0, 100);


  if (umidadeAtual <= umidadeMinima) {
    digitalWrite(RELE_PIN, LOW); 
  } else {
    digitalWrite(RELE_PIN, HIGH); 
  }
  int novoEstado = (digitalRead(RELE_PIN) == LOW) ? 1 : 0;
  if (novoEstado != releEstadoAtual) { 
    registrarLog("INFO", String(novoEstado == 1 ? "Irrigacao LIGADA" : "Irrigacao DESLIGADA") + " (umidade " + String(umidadeAtual) + "%, minima " + String(umidadeMinima, 0) + "%)");
  }
  releEstadoAtual = novoEstado;


  umidBuffer[bufferIndex] = umidadeAtual;
  bufferIndex = (bufferIndex + 1) % N_LEITURAS;
  if (totalLeituras < N_LEITURAS) totalLeituras++;
  
  float somaUmid = 0.0;
  for (int i = 0; i < totalLeituras; i++) {
    somaUmid += umidBuffer[i];
  }
  umidadeMediaAtual = (totalLeituras > 0) ? (somaUmid / totalLeituras) : 0.0;


  Serial.print("Umidade Atual: " + String(umidadeAtual) + "%");
  Serial.print(" | Mínima Definida: " + String(umidadeMinima, 0) + "%");
  Serial.println(" | Relé: " + String(releEstadoAtual == 1 ? "LIGADO" : "DESLIGADO"));
}



void setup() {
  Serial.begin(115200);
  pinMode(RELE_PIN, OUTPUT);
  digitalWrite(RELE_PIN, HIGH); 

  
  fsOk = LittleFS.begin(true); 
  if (!fsOk) Serial.println("[ERRO] Falha ao montar o LittleFS. O log ficara apenas no Monitor Serial.");
  registrarLog("INFO", "Sistema iniciado");

  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("\nTentando conectar ao Wi-Fi");

  byte tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    wifiConectado = true;
    registrarLog("INFO", "Conectado ao Wi-Fi '" + WiFi.SSID() + "'. IP: " + WiFi.localIP().toString() + " | MAC: " + WiFi.macAddress());
    iniciarServicosDeRede(); 
  } else {
    registrarLog("ERRO", "Nao foi possivel conectar ao Wi-Fi na inicializacao. Iniciando em modo autonomo; nova tentativa a cada 10 s.");
  }
}


void loop() {
  if (WiFi.status() == WL_CONNECTED && servidorIniciado) {
    server.handleClient();
  }

  if (millis() - ultimaVerificacaoWifi >= intervaloWifi) {
    ultimaVerificacaoWifi = millis();
    verificarWifi();
  }

  
  if (!horaSincronizada && time(nullptr) > 1700000000) {
    horaSincronizada = true;
    registrarLog("INFO", "Data e hora sincronizadas via NTP");
  }

  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= intervalo) {
    previousMillis = currentMillis; 
    logicaDeIrrigacao();
  }
}