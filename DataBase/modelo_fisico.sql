-- Projeto PIM II - 2o Semestre de ADS (UNIP)
-- Aluno: Pedro Henrique Duarte Barbom - RA: H57BFC3
-- Modelo Fisico do Banco de Dados - RotaSul Logistica

PRAGMA foreign_keys = ON;

-- Tabela de usuarios para login e controle de perfil
CREATE TABLE IF NOT EXISTS usuarios (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  login TEXT NOT NULL UNIQUE,
  senha TEXT NOT NULL,
  perfil TEXT NOT NULL
);

-- Tabela de motoristas cadastrados
CREATE TABLE IF NOT EXISTS motoristas (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  nome TEXT NOT NULL,
  cpf TEXT NOT NULL UNIQUE,
  cnh TEXT NOT NULL UNIQUE,
  ativo INTEGER NOT NULL DEFAULT 1
);

-- Tabela de veiculos da frota
CREATE TABLE IF NOT EXISTS veiculos (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  placa TEXT NOT NULL UNIQUE,
  modelo TEXT NOT NULL,
  capacidade_kg REAL NOT NULL,
  disponivel INTEGER NOT NULL DEFAULT 1
);

-- Tabela de fretes ligando motorista e veiculo
CREATE TABLE IF NOT EXISTS fretes (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  id_motorista INTEGER NOT NULL,
  id_veiculo INTEGER NOT NULL,
  origem INTEGER NOT NULL,
  destino INTEGER NOT NULL,
  distancia_km REAL NOT NULL,
  peso_kg REAL NOT NULL,
  valor_frete REAL NOT NULL,
  status TEXT NOT NULL,
  FOREIGN KEY (id_motorista) REFERENCES motoristas(id),
  FOREIGN KEY (id_veiculo) REFERENCES veiculos(id)
);

-- Tabela com os dados de rede, VLANs e sincronizacao da matriz e filiais
CREATE TABLE IF NOT EXISTS unidades_rede (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  codigo_regiao INTEGER NOT NULL UNIQUE,
  nome_unidade TEXT NOT NULL,
  tipo_unidade TEXT NOT NULL,
  vlan_id INTEGER NOT NULL,
  nome_vlan TEXT NOT NULL,
  subrede_ipv4 TEXT NOT NULL,
  gateway TEXT NOT NULL,
  equipamento_rede TEXT NOT NULL,
  modo_conexao TEXT NOT NULL,
  ultimo_sync TEXT NOT NULL
);

-- Tabela de parametros gerais de LGPD, nuvem e acessibilidade no console
CREATE TABLE IF NOT EXISTS config_sistema (
  id INTEGER PRIMARY KEY CHECK (id = 1),
  mascara_cpf_ativa INTEGER NOT NULL,
  senha_oculta_ativa INTEGER NOT NULL,
  modelo_nuvem TEXT NOT NULL,
  protocolo_seg TEXT NOT NULL,
  alto_contraste INTEGER NOT NULL
);

-- Insercao dos usuarios iniciais
INSERT INTO usuarios (login, senha, perfil) VALUES 
('admin', '1234', 'ADMIN'),
('operador', '1234', 'OPERADOR');

-- Insercao dos motoristas de exemplo
INSERT INTO motoristas (nome, cpf, cnh, ativo) VALUES 
('Carlos Eduardo Silva', '123.456.789-00', '98765432100', 1),
('Fernanda Oliveira Costa', '234.567.890-11', '87654321099', 1),
('Marcos Antonio Souza', '345.678.901-22', '76543210988', 1);

-- Insercao dos veiculos iniciais da frota
INSERT INTO veiculos (placa, modelo, capacidade_kg, disponivel) VALUES 
('BRA2E19', 'VW Delivery 11.180', 7500.0, 1),
('RSL4A22', 'Mercedes Accelo 1016', 3800.0, 1),
('SPU9F80', 'Volvo FH 460', 28000.0, 1);

-- Insercao dos fretes iniciais
INSERT INTO fretes (id_motorista, id_veiculo, origem, destino, distancia_km, peso_kg, valor_frete, status) VALUES 
(1, 1, 0, 1, 408.0, 1200.0, 1795.20, 'Em Transito'),
(2, 2, 1, 2, 710.0, 420.0, 2840.00, 'Entregue'),
(3, 3, 0, 3, 586.0, 6500.0, 3098.60, 'Em Coleta');

-- Insercao das unidades de rede (Matriz SP e Filiais PR, RS e MG)
INSERT INTO unidades_rede (codigo_regiao, nome_unidade, tipo_unidade, vlan_id, nome_vlan, subrede_ipv4, gateway, equipamento_rede, modo_conexao, ultimo_sync) VALUES 
(0, 'Sao Paulo (SP)', 'MATRIZ', 10, 'VLAN_OPERACIONAL', '192.168.10.0/24', '192.168.10.1', 'Firewall + Switch Gerenciavel + AP', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente'),
(1, 'Curitiba (PR)', 'FILIAL', 20, 'VLAN_OPERACIONAL', '192.168.11.0/24', '192.168.11.1', 'Roteador Borda + Firewall + Switch', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente'),
(2, 'Porto Alegre (RS)', 'FILIAL', 20, 'VLAN_OPERACIONAL', '192.168.12.0/24', '192.168.12.1', 'Roteador Borda + Firewall + Switch', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente'),
(3, 'Belo Horizonte (MG)', 'FILIAL', 20, 'VLAN_OPERACIONAL', '192.168.13.0/24', '192.168.13.1', 'Roteador Borda + Firewall + Switch', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente');

-- Insercao das configuracoes padrao do sistema
INSERT INTO config_sistema (id, mascara_cpf_ativa, senha_oculta_ativa, modelo_nuvem, protocolo_seg, alto_contraste) VALUES 
(1, 1, 1, 'IaaS / PaaS', 'HTTPS / TLS', 1);

-- Consultas utilizadas nas funcionalidades do sistema
SELECT id, nome, cpf, cnh, ativo FROM motoristas ORDER BY id;

SELECT id, placa, modelo, capacidade_kg, disponivel FROM veiculos ORDER BY id;

-- Consulta consolidada de fretes usando JOIN
SELECT f.id, m.nome, v.placa, f.origem, f.destino, f.peso_kg, f.valor_frete, f.status
FROM fretes f
JOIN motoristas m ON f.id_motorista = m.id
JOIN veiculos v ON f.id_veiculo = v.id
ORDER BY f.id;

-- Consulta das unidades e VLANs cadastradas
SELECT id, codigo_regiao, nome_unidade, tipo_unidade, vlan_id, nome_vlan, subrede_ipv4, gateway, equipamento_rede, modo_conexao, ultimo_sync
FROM unidades_rede ORDER BY codigo_regiao;

-- Atualizacao de status de frete e sincronizacao com a nuvem
UPDATE fretes SET status = 'Entregue' WHERE id = 1;
UPDATE unidades_rede SET ultimo_sync = 'Sincronizado (HTTPS/TLS)';
