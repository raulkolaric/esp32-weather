#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_BMP280.h>
#include <BH1750.h>

// Pinos I2C do ESP32
#define SDA_PIN 21
#define SCL_PIN 22

// Sensor de chuva HW-103 conectado na saída A0 ao GPIO36
#define PINO_CHUVA 36
const int CHUVA_ADC_SECO = 4095;
const int CHUVA_ADC_MOLHADO = 0;
const int AMOSTRAS_CHUVA = 10;
const float CHUVA_FATOR_SENSIBILIDADE = 1.25;

// Nome da rede Wi-Fi criada pelo ESP32
const char* ssid = "ESP32-Estacao";

// Servidor web na porta 80
WebServer servidor(80);

// Sensores
Adafruit_BMP280 bmp;
BH1750 medidorLuminosidade;

// Controle dos sensores
bool bmpOK = false;
bool bhOK = false;

// Variáveis das leituras
float temperatura = 0;
float pressao = 0;
float lux = 0;
int leituraChuvaBruta = 0;
float chuvaPercentual = 0;

String escalaLuminosidade = "Aguardando leitura";
String nivelChuva = "Aguardando leitura";

unsigned long ultimaLeitura = 0;

// Verifica se existe dispositivo I2C no endereço
bool i2cExiste(byte endereco) {
  Wire.beginTransmission(endereco);
  return Wire.endTransmission() == 0;
}

// Imprime endereço I2C no Serial Monitor
void imprimirEndereco(byte endereco) {
  Serial.print("0x");
  if (endereco < 16) Serial.print("0");
  Serial.print(endereco, HEX);
}

// Scanner I2C para diagnóstico
void scannerI2C() {
  Serial.println();
  Serial.println("Scanner I2C:");

  int encontrados = 0;

  for (byte endereco = 1; endereco < 127; endereco++) {
    if (i2cExiste(endereco)) {
      Serial.print("Dispositivo encontrado em ");
      imprimirEndereco(endereco);
      Serial.println();
      encontrados++;
    }
  }

  if (encontrados == 0) {
    Serial.println("Nenhum dispositivo I2C encontrado.");
  }

  Serial.println("Fim do scanner I2C.");
  Serial.println();
}

// Faixas ajustadas para BH1750 protegido/indireto; em sol direto, valores acima de 20000 lux são normais.
String classificarLuminosidade(float valorLux) {
  if (valorLux < 200) {
    return "Ambiente escuro";
  } 
  else if (valorLux < 800) {
    return "Sombra / pouca luz";
  } 
  else if (valorLux < 1500) {
    return "Dia nublado";
  } 
  else if (valorLux < 7000) {
    return "Dia claro";
  } 
  else if (valorLux < 20000) {
    return "Dia muito claro";
  } 
  else {
    return "Sol direto / muito ensolarado";
  }
}

// Classifica a chuva com base na intensidade percentual
String classificarChuva(float percentual) {
  if (percentual <= 7) {
    return "Sem chuva";
  } 
  else if (percentual <= 35) {
    return "Chuva fraca";
  } 
  else if (percentual <= 70) {
    return "Chuva moderada";
  } 
  else {
    return "Chuva forte";
  }
}

// Lê o HW-103 algumas vezes para reduzir oscilações da leitura analógica
int lerChuvaBruta() {
  long soma = 0;

  for (int i = 0; i < AMOSTRAS_CHUVA; i++) {
    soma += analogRead(PINO_CHUVA);
    delay(2);
  }

  return soma / AMOSTRAS_CHUVA;
}

// Faz leitura dos sensores a cada 1 segundo
void lerSensores() {
  if (millis() - ultimaLeitura < 1000) {
    return;
  }

  ultimaLeitura = millis();

  if (bmpOK) {
    temperatura = bmp.readTemperature();
    pressao = bmp.readPressure() / 100.0;   // hPa
  }

  if (bhOK) {
    lux = medidorLuminosidade.readLightLevel();

    if (lux < 0) {
      escalaLuminosidade = "Erro na leitura do BH1750";
    } else {
      escalaLuminosidade = classificarLuminosidade(lux);
    }
  } else {
    escalaLuminosidade = "Sensor de luz não encontrado";
  }

  leituraChuvaBruta = lerChuvaBruta();
  chuvaPercentual = ((float)(CHUVA_ADC_SECO - leituraChuvaBruta) * 100.0) / (CHUVA_ADC_SECO - CHUVA_ADC_MOLHADO);
  chuvaPercentual *= CHUVA_FATOR_SENSIBILIDADE;

  if (chuvaPercentual < 0) {
    chuvaPercentual = 0;
  } else if (chuvaPercentual > 100) {
    chuvaPercentual = 100;
  }

  nivelChuva = classificarChuva(chuvaPercentual);
}

// Página HTML enviada ao celular
String paginaHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Estação ESP32</title>

  <style>
    body {
      margin: 0;
      font-family: Arial, sans-serif;
      background: #101820;
      color: white;
      text-align: center;
    }

    header {
      padding: 25px 10px;
      background: #1f2d3a;
      box-shadow: 0 2px 8px rgba(0,0,0,0.4);
    }

    h1 {
      margin: 0;
      font-size: 26px;
    }

    .conteudo {
      padding: 20px;
      display: grid;
      grid-template-columns: 1fr;
      gap: 18px;
      max-width: 500px;
      margin: auto;
    }

    .cartao {
      background: #243447;
      padding: 22px;
      border-radius: 16px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.35);
    }

    .cartao-luz {
      background: #2f4a5f;
      border: 1px solid #5fa8d3;
    }

    .cartao-chuva {
      background: #2f4f4f;
      border: 1px solid #7dd3c7;
    }

    .titulo {
      font-size: 18px;
      color: #b8c7d9;
      margin-bottom: 10px;
    }

    .valor {
      font-size: 34px;
      font-weight: bold;
    }

    .valor-texto {
      font-size: 26px;
      font-weight: bold;
      color: #ffffff;
    }

    .valor-detalhe {
      margin-top: 8px;
      font-size: 18px;
      color: #b8c7d9;
    }

    .unidade {
      font-size: 18px;
      color: #b8c7d9;
    }

    .status {
      margin-top: 18px;
      font-size: 14px;
      color: #a8ffb0;
    }

    .erro {
      color: #ffb0b0;
    }
  </style>
</head>

<body>
  <header>
    <h1>Estação Meteorológica ESP32</h1>
  </header>

  <div class="conteudo">
    <div class="cartao">
      <div class="titulo">Temperatura</div>
      <div class="valor" id="temperatura">--</div>
      <div class="unidade">°C</div>
    </div>

    <div class="cartao">
      <div class="titulo">Pressão Atmosférica</div>
      <div class="valor" id="pressao">--</div>
      <div class="unidade">hPa</div>
    </div>

    <div class="cartao cartao-chuva">
      <div class="titulo">Nível de Chuva</div>
      <div class="valor-texto" id="nivelChuva">--</div>
      <div class="valor-detalhe" id="valorPercentualChuva">--%</div>
    </div>

    <div class="cartao cartao-luz">
      <div class="titulo">Escala de Luminosidade</div>
      <div class="valor-texto" id="escalaLuminosidade">--</div>
      <div class="valor-detalhe" id="valorLuxEscala">-- lux</div>
    </div>

    <div class="status" id="status">Conectado ao ESP32</div>
  </div>

  <script>
    function atualizarDados() {
      fetch('/dados')
        .then(resposta => resposta.json())
        .then(dados => {
          document.getElementById('temperatura').innerHTML = dados.bmp ? dados.temperatura.toFixed(1) : '--';
          document.getElementById('pressao').innerHTML = dados.bmp ? dados.pressao.toFixed(1) : '--';
          document.getElementById('nivelChuva').innerHTML = dados.nivelChuva;
          document.getElementById('valorPercentualChuva').innerHTML = dados.chuvaPercentual.toFixed(0) + '%';
          document.getElementById('escalaLuminosidade').innerHTML = dados.escalaLuminosidade;
          document.getElementById('valorLuxEscala').innerHTML = dados.bh ? dados.lux.toFixed(1) + ' lux' : '-- lux';

          let estado = document.getElementById('status');

          if (dados.bmp && dados.bh) {
            estado.innerHTML = 'Sensores funcionando';
            estado.className = 'status';
          } else if (dados.bmp && !dados.bh) {
            estado.innerHTML = 'BMP280 funcionando, mas BH1750 não encontrado';
            estado.className = 'status erro';
          } else if (!dados.bmp && dados.bh) {
            estado.innerHTML = 'BH1750 funcionando, mas BMP280 não encontrado';
            estado.className = 'status erro';
          } else {
            estado.innerHTML = 'Nenhum sensor encontrado';
            estado.className = 'status erro';
          }
        })
        .catch(erro => {
          document.getElementById('status').innerHTML = 'Erro ao receber dados';
          document.getElementById('status').className = 'status erro';
        });
    }

    setInterval(atualizarDados, 2000);
    atualizarDados();
  </script>
</body>
</html>
)rawliteral";

  return html;
}

// Rota principal
void tratarPaginaInicial() {
  servidor.send(200, "text/html", paginaHTML());
}

// Rota que envia os dados em JSON
void tratarDados() {
  lerSensores();

  String json = "{";

  json += "\"bmp\":";
  json += bmpOK ? "true" : "false";
  json += ",";

  json += "\"bh\":";
  json += bhOK ? "true" : "false";
  json += ",";

  json += "\"temperatura\":";
  json += String(temperatura, 2);
  json += ",";

  json += "\"pressao\":";
  json += String(pressao, 2);
  json += ",";

  json += "\"lux\":";
  json += String(lux, 2);
  json += ",";

  json += "\"leituraChuvaBruta\":";
  json += String(leituraChuvaBruta);
  json += ",";

  json += "\"chuvaPercentual\":";
  json += String(chuvaPercentual, 2);
  json += ",";

  json += "\"nivelChuva\":\"";
  json += nivelChuva;
  json += "\",";

  json += "\"escalaLuminosidade\":\"";
  json += escalaLuminosidade;
  json += "\"";

  json += "}";

  servidor.send(200, "application/json", json);
}

// Página não encontrada
void tratarPaginaNaoEncontrada() {
  servidor.send(404, "text/plain", "Página não encontrada");
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("Iniciando ESP32...");

  // Inicia I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Serial.println("I2C iniciado.");
  Serial.print("SDA: GPIO");
  Serial.println(SDA_PIN);
  Serial.print("SCL: GPIO");
  Serial.println(SCL_PIN);

  // Configura leitura analógica do sensor de chuva HW-103
  analogReadResolution(12);
  analogSetPinAttenuation(PINO_CHUVA, ADC_11db);

  Serial.print("Sensor de chuva HW-103 em GPIO");
  Serial.println(PINO_CHUVA);

  // Mostra todos os dispositivos I2C encontrados
  scannerI2C();

  // Detecta BMP280 nos endereços comuns
  Serial.println("Verificando BMP280...");

  if (i2cExiste(0x76)) {
    Serial.println("Dispositivo I2C encontrado em 0x76. Tentando iniciar BMP280...");
    if (bmp.begin(0x76)) {
      bmpOK = true;
      Serial.println("BMP280 iniciado em 0x76");
    } else {
      Serial.println("Existe algo em 0x76, mas o BMP280 não iniciou.");
    }
  } 
  else if (i2cExiste(0x77)) {
    Serial.println("Dispositivo I2C encontrado em 0x77. Tentando iniciar BMP280...");
    if (bmp.begin(0x77)) {
      bmpOK = true;
      Serial.println("BMP280 iniciado em 0x77");
    } else {
      Serial.println("Existe algo em 0x77, mas o BMP280 não iniciou.");
    }
  } 
  else {
    Serial.println("BMP280 não encontrado em 0x76 nem em 0x77");
  }

  // Detecta BH1750 nos endereços comuns
  Serial.println();
  Serial.println("Verificando BH1750...");

  delay(300);  // pequeno tempo para o sensor estabilizar

  if (i2cExiste(0x23)) {
    Serial.println("Dispositivo I2C encontrado em 0x23. Tentando iniciar BH1750...");
    if (medidorLuminosidade.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire)) {
      bhOK = true;
      Serial.println("BH1750 iniciado em 0x23");
    } else {
      Serial.println("Existe algo em 0x23, mas o BH1750 não iniciou.");
    }
  } 
  else if (i2cExiste(0x5C)) {
    Serial.println("Dispositivo I2C encontrado em 0x5C. Tentando iniciar BH1750...");
    if (medidorLuminosidade.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire)) {
      bhOK = true;
      Serial.println("BH1750 iniciado em 0x5C");
    } else {
      Serial.println("Existe algo em 0x5C, mas o BH1750 não iniciou.");
    }
  } 
  else {
    Serial.println("BH1750 não encontrado em 0x23 nem em 0x5C");
  }

  // Cria rede Wi-Fi aberta, sem senha
  Serial.println();
  Serial.println("Criando rede WiFi sem senha...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid);

  IPAddress ip = WiFi.softAPIP();

  Serial.print("Rede criada: ");
  Serial.println(ssid);

  Serial.print("Acesse pelo navegador: http://");
  Serial.println(ip);

  // Configura rotas do servidor
  servidor.on("/", tratarPaginaInicial);
  servidor.on("/dados", tratarDados);
  servidor.onNotFound(tratarPaginaNaoEncontrada);

  servidor.begin();

  Serial.println("Servidor web iniciado.");
  Serial.println("Sistema pronto.");
}

void loop() {
  servidor.handleClient();
}
