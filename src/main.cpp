#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>

int ultimoEnvio = -1; //variável que salva a última modificação no thingsboard
int valorAtual = -1;
const char* ssid = "CINGUESTS";
const char* senha = "acessocin";
const char* server= "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/telemetry"; 

// put function declarations here:
void sincronizarBomba()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Conexão Wi-Fi ativa");
    WiFiClientSecure cliente;
    cliente.setInsecure();
    HTTPClient http;

    http.begin(cliente, "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/attributes?sharedKeys=bomba_ativa,forca_bomba");
    int resultado = http.GET();

    if (resultado == 200)
    {
      String resposta = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, resposta);

      if (error)
      {
        Serial.print("Falha na leitura: ");
        Serial.println(error.f_str());
        http.end();
        return;
      }
      

      bool ativa = doc["shared"]["bomba_ativa"];
      int forca_bomba = doc["shared"]["forca_bomba"];
      Serial.println(ativa);
      Serial.println(forca_bomba);

      if (ativa == true)
      {
        valorAtual = forca_bomba; //salva o novo valor da força
      
      }else
      {
        valorAtual = 0; //zera a força anterior se a bomba for desligada
      }
      
      if (valorAtual != ultimoEnvio)
      {
        Serial2.println(valorAtual); //atualiza a esp com o que foi modificado no thingsboard
        ultimoEnvio = valorAtual;
      }
      
    
    }else
    {
      Serial.println(resultado);
    }

    http.end();
    
    
  }else
  {
    Serial.println("Conexão Wi-Fi falhou"); 
  }
  
  
}

void enviarTelemetria()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Conexão Wi-Fi ativa");
    WiFiClientSecure cliente;
    cliente.setInsecure();  
    HTTPClient http;
      
    http.begin(cliente, server);
    http.addHeader("Content-Type", "application/json");

    //variáveis para guardar as leituras
    int luminosidade_lida = 0;
    float temp_agua_lida = 0.0;
    float temp_ar_lida = 0.0;
    int umidade_lida = 0.0;
    float tensao_bat_lida = 0.0;
    bool dados_recebidos = false;

    if (Serial2.available())
    {
      String pacote_serial = Serial2.readStringUntil('\n');
      pacote_serial.trim(); //limpa espaços invisíveis

      int itens_lidos = sscanf(pacote_serial.c_str(), "L:%d,TAG:%f,TAR:%f,U:%d,B:%f", &luminosidade_lida, &temp_agua_lida, &temp_ar_lida, &umidade_lida, &tensao_bat_lida);
      
      //verifica se todos os valores foram lidos
      if (itens_lidos == 5)
      {
        dados_recebidos = true;
      }
      
    }
    
    JsonDocument pacote;

    if (dados_recebidos)
    {
      pacote["luminosidade"] = 22000;
      pacote["temp_agua"] = temp_agua_lida;
      pacote["temp_ar"] = temp_ar_lida;
      pacote["umidade"] = umidade_lida;
      pacote["nivel_bateria"] = tensao_bat_lida;
    
    } else
    {
      return;
    }
    
    pacote["ph"] = 6.9;
    pacote["ec"] = 222.8;
    pacote["nivel_reservatorio"] = 67;
    
    if (valorAtual > 0)
    {
      pacote["bomba_ativa"] = true;
      
    } else
    {
      pacote["bomba_ativa"] = false;
    }
    
    pacote["forca_bomba"] = valorAtual; 
    
    String serial_temp;
    serializeJson(pacote, serial_temp);

    int http_tempo_resposta = http.POST(serial_temp);

    if(http_tempo_resposta == 200)
    {
      Serial.println("Pacote enviado com sucesso.");
    
    }else
    {
      Serial.print("Erro HTTP: ");
      Serial.println(http_tempo_resposta);
    }
    http.end();
  }

}

void setup() {
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);
  WiFi.begin(ssid, senha);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Aguardando conexão...");
  }
  
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Conectado, chefia!");
  
}

unsigned long ultimaLeitura = 0; //varíavel de tempo da última leitura
unsigned long ultimaTelemetria = 0; //varíavel de tempo do último envio

void loop() 
{
  if (millis() - ultimaLeitura >= 2000) //quando atingir dois segundos recebe dados do thingsboard
  {
    ultimaLeitura = millis();
    sincronizarBomba();
  }

  if (Serial2.available() > 0) //quando atingir três segundos envia dados ao thingsboard
  {
    enviarTelemetria();
  }
}

