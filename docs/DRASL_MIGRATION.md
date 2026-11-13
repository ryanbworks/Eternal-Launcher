# Migração para contas Eternal (Drasl)

Roteiro para trocar o modo Nickname por contas com senha no Drasl **sem perder
inventários, bases, times, claims, estatísticas ou conquistas**. Cada fase termina
num estado estável; nada aqui desliga o servidor até a virada final.

## Decisões

| Tema | Decisão |
| --- | --- |
| Endereço | `https://accountseternal.aplot.cloud` |
| Cadastro | Aberto, sem convite |
| Modo Nickname | Sai do launcher; o servidor passa a `online-mode=true` |
| Progresso | Obrigatório manter: cada jogador conserva o **mesmo UUID** |
| Conta original (Microsoft) | Desligada no Drasl por enquanto (sem fallback Mojang); volta depois da aprovação da Microsoft |

## Por que o progresso fica intacto

O mundo guarda cada jogador pelo UUID que o modo offline calcula a partir do nickname
digitado (`OfflinePlayer:<nome>`, com maiúsculas e minúsculas). O Drasl usa exatamente
o mesmo cálculo (`PlayerUUIDGeneration = "offline"`, conferido no código da v4.0.2 e
por um teste contra o Drasl real).

`server/drasl_migrate.py` cria uma conta por jogador **com o UUID que é dono do arquivo
em `world/playerdata`**. Os caches só fornecem nomes: o `usercache.json` às vezes guarda
UUIDs da Mojang (cabeças decorativas, contas originais). O Ryan, por exemplo, estava no
cache com o UUID da conta original, mas o progresso dele está no UUID offline. Os nomes
vêm do `usercache.json`, do `usernamecache.json` e do FTB Teams (`world/ftbteams/player`).

## Infraestrutura

| Peça | Onde |
| --- | --- |
| Proxmox | `10.0.0.10` (túnel `cloudflared` gerenciado pelo painel do Cloudflare) |
| Servidor de arquivos, APIs e Wings | CT 103 `wings`, `10.0.0.13` |
| Servidor Forge (Pterodactyl) | volume `3634a66d-0346-4d72-835c-4b1298f61142` no CT 103 |
| Drasl v4.0.2 | CT 109 `accounts`, `10.0.0.17:25585`, serviço `drasl`, dados em `/var/lib/drasl` |
| Admin do Drasl | `eternal_admin`; senha e token de API em `/root/.drasl-admin` (CT 109, só root) |
| Senhas temporárias | `/root/eternal-temporary-passwords.txt` (CT 109, só root), formato `nome;senha` |

## Status

### 0. Backups
- [x] `vzdump` do CT 103 (snapshot, sem parar o jogo), protegido contra a limpeza
  automática: `hdd-1tb/dump/vzdump-lxc-103-2026_11_13-17_34_52.tar.zst` (22,4 GB).
- [ ] Na virada: backup de novo com o servidor parado.

### 1. Launcher
- [x] Login "Conta Eternal" no lugar do Nickname: token com DPAPI, senha nunca salva,
  `validate`/`refresh` antes de jogar, logout revoga o token.
- [x] Jogo com `authlib-injector` 1.2.8 (SHA-256 fixo no launcher).
- [x] Página Skins: prévia frente/costas, biblioteca, clássico/fino, usar e voltar à padrão.
- [x] Testado contra o Drasl real: login, senha errada, UUID offline, envio e leitura da
  skin, skin padrão, logout.
- [ ] Publicar a versão (fase 5).

### 2. Drasl
- [x] CT 109 criado, Drasl compilado do código (tag v4.0.2) e instalado com
  `server/drasl/install.sh`, hoje com o endereço da rede local.
- [ ] **Você:** Cloudflare Zero Trust → Networks → Tunnels → túnel do Proxmox → Public
  Hostname: `accountseternal.aplot.cloud` → `HTTP` → `10.0.0.17:25585`.
- [ ] Trocar para o domínio (no CT 109):
  `DRASL_DOMAIN=accountseternal.aplot.cloud bash /opt/eternal-drasl/install.sh`

### 3. Contas reservadas
- [x] 19 contas criadas com o UUID dono dos dados; `verify` confirmou todas.
- [ ] Entregar as senhas temporárias no privado. Cada um troca no site (Conta → senha).
- Dois arquivos de dados sem nome conhecido (`2732e5cd…`, `e45ecbdc…`, usados só em
  24/10) ficam no mundo como estão.

### 4. Release do launcher
- [ ] Publicar a versão com Conta Eternal (fluxo de release de sempre).
- [ ] Aviso no `news.json` com a data da virada.

### 5. Virada (janela combinada, ~10 minutos)
Tudo no Proxmox (`ssh root@10.0.0.10`), servidor `3634a66d…` no CT 103.
1. **Agente sem mudar o modo**: copiar `authlib-injector-1.2.8.jar` para a pasta do
   servidor e adicionar em `user_jvm_args.txt` (guardar `.bak` antes):
   `-javaagent:authlib-injector-1.2.8.jar=https://accountseternal.aplot.cloud/authlib-injector`
   Reiniciar e conferir no log `[authlib-injector]` sem erros. `online-mode` continua
   `false`: ninguém nota diferença. Isso prova que o modpack sobe com o agente.
2. Avisar no jogo e parar o servidor.
3. Backup (`vzdump` do CT 103) e `drasl_migrate.py verify` de novo (pega quem entrou
   pela primeira vez desde a reserva; rodar `reserve` para os novos).
4. `server.properties`: `online-mode=true`, `enforce-secure-profile=false` (com `.bak`).
5. Subir e entrar com duas contas; conferir inventário e posição.

### Volta atrás
1. Parar o servidor.
2. Restaurar `server.properties` e `user_jvm_args.txt` dos `.bak`.
3. Subir de novo: os jogadores voltam a entrar pelo launcher anterior.
4. Se algum dado estiver errado, restaurar o `world` do backup da virada.

## Depois da migração
- Tempo de jogo confiar na conta (o servidor de tempo valida o token no Drasl).
- Remover o mod SkinRestorer, redundante com as skins do Drasl.
- Limpar os `level<número>.dat` repetidos em `world/` (mais de 100 cópias de algum mod).
- `local-lvm` está superalocado (164 GB prometidos para ~136 GB reais): risco de disco cheio.
- Assinatura do executável (SignPath Foundation ou Certum Open Source) contra o SmartScreen.
- Aprovação da Microsoft para "Conta original" (`https://aka.ms/mce-reviewappid`); depois
  dela, decidir como religar o fallback da Mojang sem permitir nomes duplicados.
