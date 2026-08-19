///
/// \file RDSParserLocal.h
/// \brief RDS Parser - copia local derivada da biblioteca Radio (Matthias Hertel).
///
/// Baseado em RDSParser de Matthias Hertel, http://www.mathertel.de
/// (BSD style license, http://www.mathertel.de/License.aspx).
///
/// Alteracoes locais (descodificacao do RadioText, grupo 2A) face ao original:
///   * Entrega o texto de forma PROGRESSIVA: sempre que um segmento novo (ou
///     corrigido) chega, envia logo o prefixo contiguo confirmado, em vez de
///     esperar pelos 16 segmentos - a estacao pode nunca os enviar todos (ex.
///     mensagens curtas sem terminador), o que atrasava a exibicao para
///     sempre. O ecra so mostra o que ja e fiavel (nunca inventa caracteres).
///   * Respeita o terminador 0x0D (fim de mensagem) para cortar a cauda de
///     lixo de uma mensagem anterior mais longa.
///   * Confia na filtragem de erros do proprio chip (BLER por bloco, ver
///     radio_hw.cpp) em vez de exigir 2 leituras seguidas iguais por
///     segmento - isso duplicava o tempo de espera, porque cada segmento so
///     se repete uma vez por ciclo completo da mensagem (varios segundos).
///

#ifndef __RDSPARSERLOCAL_H__
#define __RDSPARSERLOCAL_H__

#include <Arduino.h>

/// callback function for passing a ServiceName, text and Time when RDS is available.
extern "C" {
  typedef void (*receiveServiceNameFunction)(const char *name);
  typedef void (*receiveTextFunction)(const char *name);
  typedef void (*receiveTimeFunction)(uint8_t hour, uint8_t minute);
}


/// Library for parsing RDS data values and extracting information.
class RDSParserLocal {
public:
  RDSParserLocal();  ///< create a new object from this class.

  /// Initialize internal variables before starting or after a change to another channel.
  void init();

  /// Pass all available RDS data through this function.
  void processData(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4);

  void attachServiceNameCallback(receiveServiceNameFunction newFunction);  ///< Register function for displaying a new Service Name.
  void attachTextCallback(receiveTextFunction newFunction);                ///< Register the function for displaying a rds text.
  void attachTimeCallback(receiveTimeFunction newFunction);                ///< Register function for displaying a new time

private:
  // ----- actual RDS values
  uint8_t rdsGroupType, rdsTP, rdsPTY;
  uint8_t _textAB, _last_textAB;

  // Program Service Name data for 2 of 3 verifications
  // assuming that error is less than 1/3 data failures.
  char _PSName1[10];            // including trailing '\00' character.
  char _PSName2[10];            // including trailing '\00' character.
  char _PSName3[10];            // including trailing '\00' character.

  char programServiceName[10];  // found station name or empty. Is max. 8 character long.
  char lastServiceName[10];     // found station name or empty. Is max. 8 character long.

  receiveServiceNameFunction _sendServiceName;  ///< Registered ServiceName function.
  receiveTimeFunction _sendTime;                ///< Registered Time function.
  receiveTextFunction _sendText;

  uint16_t _lastRDSMinutes;  ///< last RDS time send to callback.

  // ----- RadioText (grupo 2A) -----
  char     _RDSText[64 + 2];   // buffer da mensagem (max 64 chars + terminador)
  uint16_t _rdsTextRcv;        // bitmask: 1 bit por segmento (0..15) ja recebido

};  // RDSParserLocal

#endif  //__RDSPARSERLOCAL_H__
