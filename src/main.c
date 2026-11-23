/*
 * PIM II - 2o Semestre de ADS (UNIP)
 * Aluno: Pedro Henrique Duarte Barbom - RA: H57BFC3
 * Projeto: Sistema de Gestao Logistica - RotaSul Logistica
 * Descricao: Programa em C com SQLite3 para controle de usuarios, motoristas,
 * veiculos, fretes, matriz de tarifas, dados de rede/unidades e LGPD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>
#include <conio.h>
#include "sqlite3.h"

#define ARQ_BANCO "dados_transportadora.db"
#define ARQ_RELATORIO "relatorio_fretes.txt"
#define ARQ_LOG_SYNC "sync_nuvem.log"
#define MAX_ITENS 100
#define QTD_REGIOES 4
#define QTD_FAIXAS 3

/* 0 = SP (Matriz), 1 = PR (Curitiba), 2 = RS (Porto Alegre), 3 = MG (Belo Horizonte) */
const char *NOMES_REGIOES[QTD_REGIOES] = {
    "Sao Paulo (SP)",
    "Curitiba (PR)",
    "Porto Alegre (RS)",
    "Belo Horizonte (MG)"
};

typedef struct {
    int id;
    char login[30];
    char perfil[15];
    int logado;
} Sessao;

typedef struct {
    int id;
    char nome[60];
    char cpf[16];
    char cnh[16];
    int ativo;
} Motorista;

typedef struct {
    int id;
    char placa[12];
    char modelo[40];
    float capacidadeKg;
    int disponivel;
} Veiculo;

typedef struct {
    int id;
    int idMotorista;
    int idVeiculo;
    int origem;
    int destino;
    float distanciaKm;
    float pesoKg;
    float valorFrete;
    char status[20];
} Frete;

/* Guarda informacoes da matriz e das filiais para a parte de redes/nuvem */
typedef struct {
    int id;
    int codRegiao;
    char unidade[45];
    char tipo[15];
    int vlan;
    char nomeVlan[30];
    char redeIp[20];
    char gateway[18];
    char equipamentos[45];
    char conexaoNuvem[45];
    char statusSync[25];
} UnidadeRede;

/* Parametros gerais de LGPD, acessibilidade no console e nuvem */
typedef struct {
    int mascaraCpf;
    int senhaAsterisco;
    char nuvem[25];
    char protocolo[20];
    int altoContraste;
} ConfigSistema;

/* Declaracao das funcoes */
void limparTela(void);
void pausar(void);
void limparBuffer(void);
void lerLinha(char *texto, int tam);
void lerSenha(char *senha, int tam);
void montarMatrizTarifas(float matriz[QTD_REGIOES][QTD_FAIXAS]);
int abrirBanco(sqlite3 **db);
void popularBancoInicial(sqlite3 *db);
int fazerLogin(sqlite3 *db, Sessao *sessao);
void mascararCpf(const char *cpf, char *resultado);
float calcularValorFrete(const float matriz[QTD_REGIOES][QTD_FAIXAS], int origem, float km, float peso);
void cadastrarMotorista(sqlite3 *db);
void listarMotoristas(sqlite3 *db, const Sessao *sessao);
void cadastrarVeiculo(sqlite3 *db);
void listarVeiculos(sqlite3 *db);
void registrarFrete(sqlite3 *db, const float matriz[QTD_REGIOES][QTD_FAIXAS]);
void listarFretes(sqlite3 *db);
void atualizarStatusFrete(sqlite3 *db);
void exibirTarifas(const float matriz[QTD_REGIOES][QTD_FAIXAS]);
void gerarRelatorioTxt(sqlite3 *db);
void exibirUnidadesRede(sqlite3 *db);
void sincronizarNuvem(sqlite3 *db, const Sessao *sessao);
void exibirConfiguracoes(sqlite3 *db);
void menuPrincipal(sqlite3 *db, const Sessao *sessao, const float matriz[QTD_REGIOES][QTD_FAIXAS]);

int main(void) {
    /* Ajusta acentuacao no terminal do Windows */
    setlocale(LC_ALL, "Portuguese");

    sqlite3 *db = NULL;
    Sessao sessao = {0, "", "", 0};
    float matrizTarifas[QTD_REGIOES][QTD_FAIXAS];

    montarMatrizTarifas(matrizTarifas);

    if (!abrirBanco(&db)) {
        printf("Nao foi possivel abrir o banco de dados %s.\n", ARQ_BANCO);
        pausar();
        return 1;
    }

    popularBancoInicial(db);

    if (!fazerLogin(db, &sessao)) {
        printf("\nVoce errou o login 3 vezes. Programa encerrado.\n");
        sqlite3_close(db);
        pausar();
        return 1;
    }

    menuPrincipal(db, &sessao, matrizTarifas);

    sqlite3_close(db);
    return 0;
}

void limparTela(void) {
    system("cls");
}

void pausar(void) {
    printf("\nAperte ENTER para continuar...");
    getchar();
}

/* Remove o que sobrou no buffer do teclado depois de scanf */
void limparBuffer(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

void lerLinha(char *texto, int tam) {
    if (fgets(texto, tam, stdin) != NULL) {
        size_t len = strlen(texto);
        if (len > 0 && texto[len - 1] == '\n') {
            texto[len - 1] = '\0';
        } else {
            limparBuffer();
        }
    } else if (tam > 0) {
        texto[0] = '\0';
    }
}

/* Mostra '*' no lugar da senha digitada */
void lerSenha(char *senha, int tam) {
    int i = 0;
    int tecla;

    while (i < tam - 1) {
        tecla = _getch();
        if (tecla == 13 || tecla == 10) {
            break;
        } else if (tecla == 8) {
            if (i > 0) {
                i--;
                printf("\b \b");
            }
        } else if (tecla >= 32 && tecla <= 126) {
            senha[i++] = (char)tecla;
            printf("*");
        }
    }
    senha[i] = '\0';
    printf("\n");
}

/* Matriz [regiao de origem][faixa de peso: 0=ate 500kg, 1=501..2500kg, 2=acima de 2500kg] */
void montarMatrizTarifas(float matriz[QTD_REGIOES][QTD_FAIXAS]) {
    float tarifasBase[QTD_REGIOES][QTD_FAIXAS] = {
        { 3.80f, 4.40f, 5.10f },
        { 4.00f, 4.60f, 5.30f },
        { 4.20f, 4.90f, 5.60f },
        { 3.90f, 4.50f, 5.20f }
    };

    for (int i = 0; i < QTD_REGIOES; i++) {
        for (int j = 0; j < QTD_FAIXAS; j++) {
            matriz[i][j] = tarifasBase[i][j];
        }
    }
}

/* Abre o arquivo SQLite e cria as tabelas se ainda nao existirem */
int abrirBanco(sqlite3 **db) {
    int rc = sqlite3_open(ARQ_BANCO, db);
    if (rc != SQLITE_OK) {
        return 0;
    }

    const char *sql =
        "PRAGMA foreign_keys = ON;"
        "CREATE TABLE IF NOT EXISTS usuarios ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  login TEXT NOT NULL UNIQUE,"
        "  senha TEXT NOT NULL,"
        "  perfil TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS motoristas ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  nome TEXT NOT NULL,"
        "  cpf TEXT NOT NULL UNIQUE,"
        "  cnh TEXT NOT NULL UNIQUE,"
        "  ativo INTEGER NOT NULL DEFAULT 1"
        ");"
        "CREATE TABLE IF NOT EXISTS veiculos ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  placa TEXT NOT NULL UNIQUE,"
        "  modelo TEXT NOT NULL,"
        "  capacidade_kg REAL NOT NULL,"
        "  disponivel INTEGER NOT NULL DEFAULT 1"
        ");"
        "CREATE TABLE IF NOT EXISTS fretes ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  id_motorista INTEGER NOT NULL,"
        "  id_veiculo INTEGER NOT NULL,"
        "  origem INTEGER NOT NULL,"
        "  destino INTEGER NOT NULL,"
        "  distancia_km REAL NOT NULL,"
        "  peso_kg REAL NOT NULL,"
        "  valor_frete REAL NOT NULL,"
        "  status TEXT NOT NULL,"
        "  FOREIGN KEY (id_motorista) REFERENCES motoristas(id),"
        "  FOREIGN KEY (id_veiculo) REFERENCES veiculos(id)"
        ");"
        "CREATE TABLE IF NOT EXISTS unidades_rede ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  codigo_regiao INTEGER NOT NULL UNIQUE,"
        "  nome_unidade TEXT NOT NULL,"
        "  tipo_unidade TEXT NOT NULL,"
        "  vlan_id INTEGER NOT NULL,"
        "  nome_vlan TEXT NOT NULL,"
        "  subrede_ipv4 TEXT NOT NULL,"
        "  gateway TEXT NOT NULL,"
        "  equipamento_rede TEXT NOT NULL,"
        "  modo_conexao TEXT NOT NULL,"
        "  ultimo_sync TEXT NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS config_sistema ("
        "  id INTEGER PRIMARY KEY CHECK (id = 1),"
        "  mascara_cpf_ativa INTEGER NOT NULL,"
        "  senha_oculta_ativa INTEGER NOT NULL,"
        "  modelo_nuvem TEXT NOT NULL,"
        "  protocolo_seg TEXT NOT NULL,"
        "  alto_contraste INTEGER NOT NULL"
        ");";

    char *msgErro = NULL;
    rc = sqlite3_exec(*db, sql, NULL, NULL, &msgErro);
    if (rc != SQLITE_OK) {
        printf("Erro ao criar tabelas no SQLite: %s\n", msgErro);
        sqlite3_free(msgErro);
        return 0;
    }

    return 1;
}

/* Insere dados de exemplo apenas quando a tabela esta vazia */
void popularBancoInicial(sqlite3 *db) {
    sqlite3_stmt *stmt = NULL;
    int qtd = 0;

    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM usuarios;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) qtd = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (qtd == 0) {
        sqlite3_exec(db,
            "INSERT INTO usuarios (login, senha, perfil) VALUES "
            "('admin', '1234', 'ADMIN'),"
            "('operador', '1234', 'OPERADOR');",
            NULL, NULL, NULL);
    }

    qtd = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM motoristas;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) qtd = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (qtd == 0) {
        sqlite3_exec(db,
            "INSERT INTO motoristas (nome, cpf, cnh, ativo) VALUES "
            "('Carlos Eduardo Silva', '123.456.789-00', '98765432100', 1),"
            "('Fernanda Oliveira Costa', '234.567.890-11', '87654321099', 1),"
            "('Marcos Antonio Souza', '345.678.901-22', '76543210988', 1);",
            NULL, NULL, NULL);
    }

    qtd = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM veiculos;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) qtd = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (qtd == 0) {
        sqlite3_exec(db,
            "INSERT INTO veiculos (placa, modelo, capacidade_kg, disponivel) VALUES "
            "('BRA2E19', 'VW Delivery 11.180', 7500.0, 1),"
            "('RSL4A22', 'Mercedes Accelo 1016', 3800.0, 1),"
            "('SPU9F80', 'Volvo FH 460', 28000.0, 1);",
            NULL, NULL, NULL);
    }

    qtd = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM fretes;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) qtd = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (qtd == 0) {
        sqlite3_exec(db,
            "INSERT INTO fretes (id_motorista, id_veiculo, origem, destino, distancia_km, peso_kg, valor_frete, status) VALUES "
            "(1, 1, 0, 1, 408.0, 1200.0, 1795.20, 'Em Transito'),"
            "(2, 2, 1, 2, 710.0, 420.0, 2840.00, 'Entregue'),"
            "(3, 3, 0, 3, 586.0, 6500.0, 2839.17, 'Em Coleta');",
            NULL, NULL, NULL);
    }

    qtd = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM unidades_rede;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) qtd = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (qtd == 0) {
        sqlite3_exec(db,
            "INSERT INTO unidades_rede (codigo_regiao, nome_unidade, tipo_unidade, vlan_id, nome_vlan, subrede_ipv4, gateway, equipamento_rede, modo_conexao, ultimo_sync) VALUES "
            "(0, 'Sao Paulo (SP)', 'MATRIZ', 10, 'VLAN_OPERACIONAL', '192.168.10.0/24', '192.168.10.1', 'Firewall + Switch Gerenciavel + AP', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente'),"
            "(1, 'Curitiba (PR)', 'FILIAL', 20, 'VLAN_OPERACIONAL', '192.168.11.0/24', '192.168.11.1', 'Roteador Borda + Firewall + Switch', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente'),"
            "(2, 'Porto Alegre (RS)', 'FILIAL', 20, 'VLAN_OPERACIONAL', '192.168.12.0/24', '192.168.12.1', 'Roteador Borda + Firewall + Switch', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente'),"
            "(3, 'Belo Horizonte (MG)', 'FILIAL', 20, 'VLAN_OPERACIONAL', '192.168.13.0/24', '192.168.13.1', 'Roteador Borda + Firewall + Switch', 'VPN Site-to-Site + HTTPS/TLS', 'Pendente');",
            NULL, NULL, NULL);
    }

    qtd = 0;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM config_sistema;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) qtd = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
    }
    if (qtd == 0) {
        sqlite3_exec(db,
            "INSERT INTO config_sistema (id, mascara_cpf_ativa, senha_oculta_ativa, modelo_nuvem, protocolo_seg, alto_contraste) VALUES "
            "(1, 1, 1, 'IaaS / PaaS', 'HTTPS / TLS', 1);",
            NULL, NULL, NULL);
    }
}

int fazerLogin(sqlite3 *db, Sessao *sessao) {
    char login[30];
    char senha[30];
    int tentativas = 0;
    const int limite = 3;

    sessao->logado = 0;

    while (tentativas < limite) {
        limparTela();
        printf("=========================================================\n\n");
        printf("   SISTEMA DE GESTAO LOGISTICA - PIM II (ADS/UNIP)     \n");
        printf("   Desenvolvido por: Pedro Henrique Duarte Barbom - H57BFC3  \n");
        printf("=========================================================\n\n");
        printf("=== ROTASUL LOGISTICA - LOGIN ===\n");
        printf("(Teste com admin/1234 ou operador/1234)\n\n");
        printf("Usuario: ");
        lerLinha(login, sizeof(login));

        printf("Senha: ");
        lerSenha(senha, sizeof(senha));

        sqlite3_stmt *stmt = NULL;
        const char *sql = "SELECT id, perfil FROM usuarios WHERE login = ? AND senha = ?;";

        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, login, -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, senha, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) == SQLITE_ROW) {
                sessao->id = sqlite3_column_int(stmt, 0);
                const char *perfil = (const char *)sqlite3_column_text(stmt, 1);
                strcpy(sessao->login, login);
                strcpy(sessao->perfil, perfil ? perfil : "OPERADOR");
                sessao->logado = 1;
                sqlite3_finalize(stmt);
                return 1;
            }
            sqlite3_finalize(stmt);
        }

        tentativas++;
        printf("\nLogin ou senha incorretos. Tentativa %d de %d.\n", tentativas, limite);
        pausar();
    }

    return 0;
}

/* Mascara o CPF para atender a LGPD na tela e nos relatorios */
void mascararCpf(const char *cpf, char *resultado) {
    if (cpf != NULL && strlen(cpf) >= 14 && cpf[3] == '.' && cpf[7] == '.' && cpf[11] == '-') {
        snprintf(resultado, 16, "***.%c%c%c.%c%c%c-**",
                 cpf[4], cpf[5], cpf[6],
                 cpf[8], cpf[9], cpf[10]);
    } else {
        strcpy(resultado, "***.***.***-**");
    }
}

float calcularValorFrete(const float matriz[QTD_REGIOES][QTD_FAIXAS], int origem, float km, float peso) {
    int faixa = 0;
    if (peso <= 500.0f) {
        faixa = 0;
    } else if (peso <= 2500.0f) {
        faixa = 1;
    } else {
        faixa = 2;
    }

    float valor = km * matriz[origem][faixa];
    if (peso > 5000.0f) {
        valor *= 0.95f;
    }
    return valor;
}

void cadastrarMotorista(sqlite3 *db) {
    Motorista m;
    m.ativo = 1;

    limparTela();
    printf("=== CADASTRAR MOTORISTA ===\n\n");
    printf("Nome: ");
    lerLinha(m.nome, sizeof(m.nome));

    do {
        printf("CPF (000.000.000-00): ");
        lerLinha(m.cpf, sizeof(m.cpf));
        if (strlen(m.cpf) != 14) {
            printf("CPF precisa ter 14 caracteres no formato 000.000.000-00.\n");
        }
    } while (strlen(m.cpf) != 14);

    printf("CNH: ");
    lerLinha(m.cnh, sizeof(m.cnh));

    if (strlen(m.nome) == 0 || strlen(m.cnh) < 5) {
        printf("\nPreencha nome e CNH corretamente.\n");
        pausar();
        return;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO motoristas (nome, cpf, cnh, ativo) VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, m.nome, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, m.cpf, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, m.cnh, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, m.ativo);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            printf("\nMotorista salvo com sucesso!\n");
        } else {
            printf("\nNao foi possivel salvar (CPF ou CNH ja existe).\n");
        }
        sqlite3_finalize(stmt);
    }

    pausar();
}

void listarMotoristas(sqlite3 *db, const Sessao *sessao) {
    Motorista lista[MAX_ITENS];
    int total = 0;
    int mostrarCpf = 0;

    limparTela();
    printf("=== LISTA DE MOTORISTAS (LGPD) ===\n\n");

    if (strcmp(sessao->perfil, "ADMIN") == 0) {
        char resp;
        printf("Mostrar CPF completo? (S/N): ");
        scanf(" %c", &resp);
        limparBuffer();
        if (toupper((unsigned char)resp) == 'S') {
            mostrarCpf = 1;
        }
        printf("\n");
    } else {
        printf("Perfil OPERADOR: CPF mascarado por padrao.\n\n");
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, nome, cpf, cnh, ativo FROM motoristas ORDER BY id;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (total < MAX_ITENS && sqlite3_step(stmt) == SQLITE_ROW) {
            lista[total].id = sqlite3_column_int(stmt, 0);
            snprintf(lista[total].nome, sizeof(lista[total].nome), "%s",
                     (const char *)sqlite3_column_text(stmt, 1));
            snprintf(lista[total].cpf, sizeof(lista[total].cpf), "%s",
                     (const char *)sqlite3_column_text(stmt, 2));
            snprintf(lista[total].cnh, sizeof(lista[total].cnh), "%s",
                     (const char *)sqlite3_column_text(stmt, 3));
            lista[total].ativo = sqlite3_column_int(stmt, 4);
            total++;
        }
        sqlite3_finalize(stmt);
    }

    printf("%-4s | %-26s | %-15s | %-12s | %-7s\n", "ID", "Nome", "CPF", "CNH", "Ativo");
    printf("---------------------------------------------------------------------------\n");

    for (int i = 0; i < total; i++) {
        char cpfFormatado[16];
        if (mostrarCpf) {
            snprintf(cpfFormatado, sizeof(cpfFormatado), "%s", lista[i].cpf);
        } else {
            mascararCpf(lista[i].cpf, cpfFormatado);
        }
        printf("%-4d | %-26.26s | %-15s | %-12s | %-7s\n",
               lista[i].id, lista[i].nome, cpfFormatado, lista[i].cnh,
               lista[i].ativo ? "Sim" : "Nao");
    }

    pausar();
}

void cadastrarVeiculo(sqlite3 *db) {
    Veiculo v;
    v.disponivel = 1;

    limparTela();
    printf("=== CADASTRAR VEICULO ===\n\n");
    printf("Placa: ");
    lerLinha(v.placa, sizeof(v.placa));
    for (int i = 0; v.placa[i]; i++) {
        v.placa[i] = (char)toupper((unsigned char)v.placa[i]);
    }

    printf("Modelo: ");
    lerLinha(v.modelo, sizeof(v.modelo));

    printf("Capacidade (kg): ");
    if (scanf("%f", &v.capacidadeKg) != 1 || v.capacidadeKg <= 0.0f) {
        limparBuffer();
        printf("\nCapacidade invalida.\n");
        pausar();
        return;
    }
    limparBuffer();

    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO veiculos (placa, modelo, capacidade_kg, disponivel) VALUES (?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, v.placa, -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, v.modelo, -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, v.capacidadeKg);
        sqlite3_bind_int(stmt, 4, v.disponivel);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            printf("\nVeiculo cadastrado!\n");
        } else {
            printf("\nErro ao cadastrar veiculo (placa ja cadastrada).\n");
        }
        sqlite3_finalize(stmt);
    }

    pausar();
}

void listarVeiculos(sqlite3 *db) {
    limparTela();
    printf("=== LISTA DE VEICULOS ===\n\n");

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, placa, modelo, capacidade_kg, disponivel FROM veiculos ORDER BY id;";

    printf("%-4s | %-9s | %-24s | %-12s | %-10s\n",
           "ID", "Placa", "Modelo", "Capac.(kg)", "Status");
    printf("-------------------------------------------------------------------------\n");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            printf("%-4d | %-9s | %-24.24s | %-12.1f | %-10s\n",
                   sqlite3_column_int(stmt, 0),
                   (const char *)sqlite3_column_text(stmt, 1),
                   (const char *)sqlite3_column_text(stmt, 2),
                   sqlite3_column_double(stmt, 3),
                   sqlite3_column_int(stmt, 4) ? "Disponivel" : "Ocupado");
        }
        sqlite3_finalize(stmt);
    }

    pausar();
}

void registrarFrete(sqlite3 *db, const float matriz[QTD_REGIOES][QTD_FAIXAS]) {
    Frete f;

    limparTela();
    printf("=== REGISTRAR FRETE ===\n\n");

    printf("ID do motorista: ");
    if (scanf("%d", &f.idMotorista) != 1) {
        limparBuffer();
        printf("ID invalido.\n");
        pausar();
        return;
    }

    printf("ID do veiculo: ");
    if (scanf("%d", &f.idVeiculo) != 1) {
        limparBuffer();
        printf("ID invalido.\n");
        pausar();
        return;
    }

    printf("\n0 = SP (Matriz) | 1 = PR | 2 = RS | 3 = MG\n");
    printf("Origem (0-3): ");
    scanf("%d", &f.origem);
    printf("Destino (0-3): ");
    scanf("%d", &f.destino);
    printf("Distancia (km): ");
    scanf("%f", &f.distanciaKm);
    printf("Peso (kg): ");
    scanf("%f", &f.pesoKg);
    limparBuffer();

    if (f.origem < 0 || f.origem >= QTD_REGIOES || f.destino < 0 || f.destino >= QTD_REGIOES ||
        f.distanciaKm <= 0.0f || f.pesoKg <= 0.0f) {
        printf("\nDados invalidos.\n");
        pausar();
        return;
    }

    f.valorFrete = calcularValorFrete(matriz, f.origem, f.distanciaKm, f.pesoKg);
    strcpy(f.status, "Em Coleta");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO fretes (id_motorista, id_veiculo, origem, destino, distancia_km, peso_kg, valor_frete, status) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, f.idMotorista);
        sqlite3_bind_int(stmt, 2, f.idVeiculo);
        sqlite3_bind_int(stmt, 3, f.origem);
        sqlite3_bind_int(stmt, 4, f.destino);
        sqlite3_bind_double(stmt, 5, f.distanciaKm);
        sqlite3_bind_double(stmt, 6, f.pesoKg);
        sqlite3_bind_double(stmt, 7, f.valorFrete);
        sqlite3_bind_text(stmt, 8, f.status, -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            printf("\nFrete registrado! Valor: R$ %.2f\n", f.valorFrete);
        } else {
            printf("\nErro ao salvar frete. Confira se o motorista e o veiculo existem.\n");
        }
        sqlite3_finalize(stmt);
    }

    pausar();
}

void listarFretes(sqlite3 *db) {
    limparTela();
    printf("=== LISTA DE FRETES ===\n\n");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT f.id, m.nome, v.placa, f.origem, f.destino, f.peso_kg, f.valor_frete, f.status "
        "FROM fretes f "
        "JOIN motoristas m ON f.id_motorista = m.id "
        "JOIN veiculos v ON f.id_veiculo = v.id "
        "ORDER BY f.id;";

    printf("%-4s | %-18s | %-8s | %-25s | %-8s | %-10s | %-12s\n",
           "ID", "Motorista", "Placa", "Origem -> Destino", "Peso", "Valor(R$)", "Status");
    printf("-------------------------------------------------------------------------------------------------\n");

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int orig = sqlite3_column_int(stmt, 3);
            int dest = sqlite3_column_int(stmt, 4);
            char rota[45];
            snprintf(rota, sizeof(rota), "%s -> %s",
                     (orig >= 0 && orig < QTD_REGIOES) ? NOMES_REGIOES[orig] : "?",
                     (dest >= 0 && dest < QTD_REGIOES) ? NOMES_REGIOES[dest] : "?");

            printf("%-4d | %-18.18s | %-8s | %-25.25s | %-8.1f | %-10.2f | %-12s\n",
                   sqlite3_column_int(stmt, 0),
                   (const char *)sqlite3_column_text(stmt, 1),
                   (const char *)sqlite3_column_text(stmt, 2),
                   rota,
                   sqlite3_column_double(stmt, 5),
                   sqlite3_column_double(stmt, 6),
                   (const char *)sqlite3_column_text(stmt, 7));
        }
        sqlite3_finalize(stmt);
    }

    pausar();
}

void atualizarStatusFrete(sqlite3 *db) {
    int id = 0;
    int op = 0;
    const char *novoStatus = NULL;

    limparTela();
    printf("=== ATUALIZAR STATUS DO FRETE ===\n\n");
    printf("ID do frete: ");
    if (scanf("%d", &id) != 1) {
        limparBuffer();
        printf("ID invalido.\n");
        pausar();
        return;
    }

    printf("1=Em Coleta | 2=Em Transito | 3=Entregue | 4=Cancelado: ");
    if (scanf("%d", &op) != 1) {
        limparBuffer();
        printf("Opcao invalida.\n");
        pausar();
        return;
    }
    limparBuffer();

    switch (op) {
        case 1: novoStatus = "Em Coleta"; break;
        case 2: novoStatus = "Em Transito"; break;
        case 3: novoStatus = "Entregue"; break;
        case 4: novoStatus = "Cancelado"; break;
        default:
            printf("Status invalido.\n");
            pausar();
            return;
    }

    sqlite3_stmt *stmt = NULL;
    const char *sql = "UPDATE fretes SET status = ? WHERE id = ?;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, novoStatus, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, id);

        if (sqlite3_step(stmt) == SQLITE_DONE && sqlite3_changes(db) > 0) {
            printf("\nStatus atualizado para %s.\n", novoStatus);
        } else {
            printf("\nFrete nao encontrado.\n");
        }
        sqlite3_finalize(stmt);
    }

    pausar();
}

void exibirTarifas(const float matriz[QTD_REGIOES][QTD_FAIXAS]) {
    limparTela();
    printf("=== TARIFAS BASE POR KM (R$) ===\n\n");
    printf("%-20s | %-14s | %-16s | %-15s\n",
           "Origem", "Ate 500 kg", "501 a 2500 kg", "Acima 2500 kg");
    printf("---------------------------------------------------------------------------\n");

    for (int i = 0; i < QTD_REGIOES; i++) {
        printf("%-20s | R$ %-11.2f | R$ %-13.2f | R$ %-12.2f\n",
               NOMES_REGIOES[i], matriz[i][0], matriz[i][1], matriz[i][2]);
    }

    pausar();
}

void gerarRelatorioTxt(sqlite3 *db) {
    limparTela();
    printf("=== GERAR RELATORIO TXT ===\n\n");

    FILE *arq = fopen(ARQ_RELATORIO, "w");
    if (arq == NULL) {
        printf("Erro ao criar %s.\n", ARQ_RELATORIO);
        pausar();
        return;
    }

    fprintf(arq, "ROTASUL LOGISTICA - RELATORIO DE FRETES\n");
    fprintf(arq, "========================================================================\n");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT f.id, m.nome, m.cpf, v.placa, f.origem, f.destino, f.distancia_km, f.peso_kg, f.valor_frete, f.status "
        "FROM fretes f "
        "JOIN motoristas m ON f.id_motorista = m.id "
        "JOIN veiculos v ON f.id_veiculo = v.id "
        "ORDER BY f.id;";

    float soma = 0.0f;
    int linhas = 0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int(stmt, 0);
            const char *nome = (const char *)sqlite3_column_text(stmt, 1);
            const char *cpf = (const char *)sqlite3_column_text(stmt, 2);
            const char *placa = (const char *)sqlite3_column_text(stmt, 3);
            int orig = sqlite3_column_int(stmt, 4);
            int dest = sqlite3_column_int(stmt, 5);
            double km = sqlite3_column_double(stmt, 6);
            double peso = sqlite3_column_double(stmt, 7);
            double valor = sqlite3_column_double(stmt, 8);
            const char *status = (const char *)sqlite3_column_text(stmt, 9);

            char cpfMasc[16];
            mascararCpf(cpf, cpfMasc);

            fprintf(arq,
                    "Frete #%d | Motorista: %s (CPF: %s) | Veiculo: %s\n"
                    "Rota: %s -> %s | %.1f km | %.1f kg | R$ %.2f | %s\n"
                    "------------------------------------------------------------------------\n",
                    id, nome, cpfMasc, placa,
                    (orig >= 0 && orig < QTD_REGIOES) ? NOMES_REGIOES[orig] : "?",
                    (dest >= 0 && dest < QTD_REGIOES) ? NOMES_REGIOES[dest] : "?",
                    km, peso, valor, status);

            soma += (float)valor;
            linhas++;
        }
        sqlite3_finalize(stmt);
    }

    fprintf(arq, "\nTotal de fretes: %d | Soma dos valores: R$ %.2f\n", linhas, soma);
    fclose(arq);

    printf("Arquivo '%s' gerado com %d fretes.\n", ARQ_RELATORIO, linhas);
    pausar();
}

void exibirUnidadesRede(sqlite3 *db) {
    UnidadeRede lista[QTD_REGIOES];
    int total = 0;

    limparTela();
    printf("=== UNIDADES, VLANs E REDE ===\n\n");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id, codigo_regiao, nome_unidade, tipo_unidade, vlan_id, nome_vlan, "
        "subrede_ipv4, gateway, equipamento_rede, modo_conexao, ultimo_sync "
        "FROM unidades_rede ORDER BY codigo_regiao;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        while (total < QTD_REGIOES && sqlite3_step(stmt) == SQLITE_ROW) {
            lista[total].id = sqlite3_column_int(stmt, 0);
            lista[total].codRegiao = sqlite3_column_int(stmt, 1);
            snprintf(lista[total].unidade, sizeof(lista[total].unidade), "%s",
                     (const char *)sqlite3_column_text(stmt, 2));
            snprintf(lista[total].tipo, sizeof(lista[total].tipo), "%s",
                     (const char *)sqlite3_column_text(stmt, 3));
            lista[total].vlan = sqlite3_column_int(stmt, 4);
            snprintf(lista[total].nomeVlan, sizeof(lista[total].nomeVlan), "%s",
                     (const char *)sqlite3_column_text(stmt, 5));
            snprintf(lista[total].redeIp, sizeof(lista[total].redeIp), "%s",
                     (const char *)sqlite3_column_text(stmt, 6));
            snprintf(lista[total].gateway, sizeof(lista[total].gateway), "%s",
                     (const char *)sqlite3_column_text(stmt, 7));
            snprintf(lista[total].equipamentos, sizeof(lista[total].equipamentos), "%s",
                     (const char *)sqlite3_column_text(stmt, 8));
            snprintf(lista[total].conexaoNuvem, sizeof(lista[total].conexaoNuvem), "%s",
                     (const char *)sqlite3_column_text(stmt, 9));
            snprintf(lista[total].statusSync, sizeof(lista[total].statusSync), "%s",
                     (const char *)sqlite3_column_text(stmt, 10));
            total++;
        }
        sqlite3_finalize(stmt);
    }

    for (int i = 0; i < total; i++) {
        printf("[%s] %s\n", lista[i].tipo, lista[i].unidade);
        printf("  VLAN %d (%s) | Rede: %s | Gateway: %s\n",
               lista[i].vlan, lista[i].nomeVlan, lista[i].redeIp, lista[i].gateway);
        printf("  Equipamentos: %s\n", lista[i].equipamentos);
        printf("  Conexao: %s | Sync: %s\n\n", lista[i].conexaoNuvem, lista[i].statusSync);
    }

    pausar();
}

void sincronizarNuvem(sqlite3 *db, const Sessao *sessao) {
    limparTela();
    printf("=== SINCRONIZACAO COM A NUVEM ===\n\n");

    const char *sql = "UPDATE unidades_rede SET ultimo_sync = 'Sincronizado (HTTPS/TLS)';";
    if (sqlite3_exec(db, sql, NULL, NULL, NULL) != SQLITE_OK) {
        printf("Erro ao atualizar sincronizacao.\n");
        pausar();
        return;
    }

    FILE *log = fopen(ARQ_LOG_SYNC, "a");
    if (log != NULL) {
        fprintf(log, "Usuario: %s (%s) | Banco: %s | Canal: VPN Site-to-Site + HTTPS/TLS | OK\n",
                sessao->login, sessao->perfil, ARQ_BANCO);
        fclose(log);
    }

    printf("Unidades atualizadas e log gravado em '%s'.\n", ARQ_LOG_SYNC);
    pausar();
}

void exibirConfiguracoes(sqlite3 *db) {
    ConfigSistema cfg = {1, 1, "IaaS / PaaS", "HTTPS / TLS", 1};

    limparTela();
    printf("=== PARAMETROS DO SISTEMA ===\n\n");

    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT mascara_cpf_ativa, senha_oculta_ativa, modelo_nuvem, protocolo_seg, alto_contraste "
        "FROM config_sistema WHERE id = 1;";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            cfg.mascaraCpf = sqlite3_column_int(stmt, 0);
            cfg.senhaAsterisco = sqlite3_column_int(stmt, 1);
            snprintf(cfg.nuvem, sizeof(cfg.nuvem), "%s",
                     (const char *)sqlite3_column_text(stmt, 2));
            snprintf(cfg.protocolo, sizeof(cfg.protocolo), "%s",
                     (const char *)sqlite3_column_text(stmt, 3));
            cfg.altoContraste = sqlite3_column_int(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }

    printf("Banco local         : %s\n", ARQ_BANCO);
    printf("Mascara CPF (LGPD)  : %s\n", cfg.mascaraCpf ? "Ativa (***.456.789-**)" : "Desativada");
    printf("Senha com '*'       : %s\n", cfg.senhaAsterisco ? "Ativa" : "Desativada");
    printf("Modelo de nuvem     : %s\n", cfg.nuvem);
    printf("Protocolo seguro    : %s\n", cfg.protocolo);
    printf("Visual no console   : %s\n", cfg.altoContraste ? "Limpeza de tela e pausas ativas" : "Padrao");

    pausar();
}

void menuPrincipal(sqlite3 *db, const Sessao *sessao, const float matriz[QTD_REGIOES][QTD_FAIXAS]) {
    int opcao = 0;

    do {
        limparTela();
        printf("=========================================================\n\n");
        printf("   SISTEMA DE GESTAO LOGISTICA - PIM II (ADS/UNIP)     \n");
        printf("   Desenvolvido por: Pedro Henrique Duarte Barbom - H57BFC3  \n");
        printf("=========================================================\n\n");
        printf("   ROTASUL LOGISTICA - MENU PRINCIPAL\n");
        printf("   Usuario: %-15s | Perfil: %-10s\n", sessao->login, sessao->perfil);
        printf("====================================================\n");
        printf("1. Cadastrar Motorista\n");
        printf("2. Listar Motoristas (LGPD)\n");
        printf("3. Cadastrar Veiculo\n");
        printf("4. Listar Veiculos\n");
        printf("5. Registrar Frete\n");
        printf("6. Listar Fretes\n");
        printf("7. Atualizar Status do Frete\n");
        printf("8. Ver Matriz de Tarifas\n");
        printf("9. Gerar Relatorio TXT\n");
        printf("10. Ver Unidades, VLANs e Rede\n");
        printf("11. Sincronizar com a Nuvem\n");
        printf("12. Ver Parametros (LGPD / Nuvem)\n");
        printf("0. Sair\n");
        printf("Escolha uma opcao: ");

        if (scanf("%d", &opcao) != 1) {
            limparBuffer();
            printf("\nDigite apenas numeros.\n");
            pausar();
            opcao = -1;
            continue;
        }
        limparBuffer();

        switch (opcao) {
            case 1: cadastrarMotorista(db); break;
            case 2: listarMotoristas(db, sessao); break;
            case 3: cadastrarVeiculo(db); break;
            case 4: listarVeiculos(db); break;
            case 5: registrarFrete(db, matriz); break;
            case 6: listarFretes(db); break;
            case 7: atualizarStatusFrete(db); break;
            case 8: exibirTarifas(matriz); break;
            case 9: gerarRelatorioTxt(db); break;
            case 10: exibirUnidadesRede(db); break;
            case 11: sincronizarNuvem(db, sessao); break;
            case 12: exibirConfiguracoes(db); break;
            case 0:
                limparTela();
                printf("Saindo do sistema. Dados salvos em %s.\n", ARQ_BANCO);
                break;
            default:
                printf("\nOpcao invalida.\n");
                pausar();
                break;
        }
    } while (opcao != 0);
}
