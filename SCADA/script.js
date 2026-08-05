// Altere esta URL para o endpoint real da sua API
const API_URL = "http://localhost:5000/leituras";

// Intervalo de atualização em milissegundos
const INTERVALO_ATUALIZACAO = 2000;

// Guarda as últimas leituras para mostrar na tabela
let historico = [];

// Função principal: busca os dados da API
async function buscarDadosDaApi() {
  try {
    alterarStatusConexao("Conectando...", "conectando");
    limparErro();

    const resposta = await fetch(API_URL, {
      method: "GET",
      cache: "no-store"
    });

    if (!resposta.ok) {
      throw new Error(`Erro HTTP: ${resposta.status}`);
    }

    const dadosRecebidos = await resposta.json();

    const dados = normalizarDados(dadosRecebidos);

    atualizarCards(dados);
    verificarAlarmes(dados);
    adicionarNoHistorico(dados);

    alterarStatusConexao("Online", "online");
    document.getElementById("statusLeitura").textContent = "Dados atualizados com sucesso.";
  } catch (erro) {
    alterarStatusConexao("Offline", "offline");

    document.getElementById("statusLeitura").textContent = "Falha ao atualizar dados.";
    document.getElementById("mensagemErro").textContent =
      `Erro ao buscar dados da API: ${erro.message}`;
  }
}

// Normaliza os dados para evitar erro caso a API retorne nomes diferentes
function normalizarDados(dadosRecebidos) {
  let dados = dadosRecebidos;

  // Se a API retornar uma lista, pega o último registro
  if (Array.isArray(dadosRecebidos)) {
    dados = dadosRecebidos[dadosRecebidos.length - 1];
  }

  return {
    ph: Number(dados.ph ?? 0),
    temperatura: Number(dados.temperatura ?? 0),
    orp: Number(dados.orp ?? 0),
    turbidez: Number(dados.turbidez ?? 0),
    created_at: dados.data_hora ?? new Date().toISOString()
  };
}

// Atualiza os cards principais da tela
function atualizarCards(dados) {
  document.getElementById("valorPh").textContent = dados.ph.toFixed(2);
  document.getElementById("valorTemperatura").textContent = `${dados.temperatura.toFixed(1)} °C`;
  document.getElementById("valorOrp").textContent = `${dados.orp.toFixed(0)} mV`;
  document.getElementById("valorTurbidez").textContent = `${dados.turbidez.toFixed(1)} NTU`;

  document.getElementById("dataLeitura").textContent = formatarData(dados.created_at);
}

// Verifica condições de alerta
function verificarAlarmes(dados) {
  const alerta = document.getElementById("alertaSistema");
  let mensagens = [];

  // Ajuste esses limites conforme os requisitos do projeto
  if (dados.ph < 6.5) {
    mensagens.push("pH abaixo do ideal. Água muito ácida.");
  }

  if (dados.ph > 8.5) {
    mensagens.push("pH acima do ideal. Água muito básica.");
  }

  if (dados.temperatura < 20 || dados.temperatura > 30) {
    mensagens.push("Temperatura fora da faixa recomendada.");
  }

  if (dados.orp < 250) {
    mensagens.push("ORP baixo. Possível baixa capacidade de oxidação.");
  }

  if (dados.turbidez > 50) {
    mensagens.push("Turbidez elevada. Água com excesso de partículas.");
  }

  if (mensagens.length > 0) {
    alerta.classList.remove("oculto");
    alerta.textContent = mensagens.join(" ");
  } else {
    alerta.classList.add("oculto");
    alerta.textContent = "Nenhum alerta no momento.";
  }
}

// Adiciona leitura ao histórico
function adicionarNoHistorico(dados) {
  historico.unshift(dados);

  // Mantém apenas as últimas 10 leituras
  if (historico.length > 10) {
    historico.pop();
  }

  atualizarTabelaHistorico();
}

// Atualiza a tabela de histórico
function atualizarTabelaHistorico() {
  const tabela = document.getElementById("tabelaHistorico");

  tabela.innerHTML = "";

  historico.forEach((item) => {
    const linha = document.createElement("tr");

    linha.innerHTML = `
      <td>${formatarData(item.created_at)}</td>
      <td>${item.ph.toFixed(2)}</td>
      <td>${item.temperatura.toFixed(1)} °C</td>
      <td>${item.orp.toFixed(0)} mV</td>
      <td>${item.turbidez.toFixed(1)} NTU</td>
    `;

    tabela.appendChild(linha);
  });
}

// Atualiza o status de conexão na tela
function alterarStatusConexao(texto, classe) {
  const status = document.getElementById("statusConexao");

  status.textContent = texto;

  status.classList.remove("online", "offline", "conectando");
  status.classList.add(classe);
}

// Limpa mensagem de erro
function limparErro() {
  document.getElementById("mensagemErro").textContent = "";
}

// Formata data para padrão brasileiro
function formatarData(data) {
  const dataObj = new Date(data);

  if (isNaN(dataObj.getTime())) {
    return "--";
  }

  return dataObj.toLocaleString("pt-BR");
}

// Busca uma vez assim que a página abre
buscarDadosDaApi();

// Depois continua buscando em tempo real
setInterval(buscarDadosDaApi, INTERVALO_ATUALIZACAO);