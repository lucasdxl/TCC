# Sistema de Monitoramento Inteligente da Qualidade da Água de Reservatórios

Projeto de TCC voltado ao monitoramento de parâmetros da água utilizando ESP32, API em Python, banco MySQL e dashboard em Power BI.

## Tecnologias
- ESP32
- C/C++
- Python
- Flask/FastAPI
- MySQL
- Power BI

## Estrutura do projeto
- `firmware/esp32`: código do microcontrolador
- `backend/api_python`: API para recebimento e armazenamento dos dados
- `database`: scripts SQL
- `dashboard`: arquivos e scripts de visualização
- `docs`: documentação do projeto

## Fluxo do sistema
Sensores -> ESP32 -> API Python -> MySQL -> Dashboard

## Parâmetros monitorados
- pH
- Turbidez
- Temperatura
- ORP ou oxigênio dissolvido
- Alcalinidade por titulação assistida

## Autores
- João Henrique Tomaz Dutra
- Lucas Balint Vilar

# Professor Orientador
- Marcelo do Carmo Gaiotto
