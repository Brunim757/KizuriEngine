# Fase 4 — Pipeline de Assets: Fluxo Detalhado

**Baseado em:** `Kizuri_Engine_TDD.md` e `Kizuri_Engine_Roadmap.md`, Fase 4.
**Objetivo deste documento:** deixar explícito, passo a passo, como cada parte do pipeline de assets funciona — pra você e pra qualquer agente de IA executando a fase não terem dúvida de comportamento.

**Decisão registrada nesta versão** (não estava fechada antes): um `.kzmesh` colocado direto no projeto, sem `.glb` de origem, **é aceito** e funciona normalmente — mas fica marcado no Asset Browser como "sem fonte de reimport" (ver Seção 5).

---

## 1. Princípio geral

`.glTF`/`.glb` é formato de **troca** (Blender, Maya, qualquer ferramenta externa consegue exportar). `.kzmesh` é o formato **nativo** da engine (FlatBuffers, zero-copy, já no layout que a engine usa).

**Regra fixa:** em runtime (jogo rodando, cena carregando), a engine **nunca** lê `.glb`. Só lê `.kzmesh`. O `.glb` só é tocado no momento do import, dentro do Editor.

---

## 2. Fluxo de Import — Mesh (`.glb` → `.kzmesh`)

```
1. Artista exporta .glb do Blender
2. Arquivo .glb entra na pasta Assets/ do projeto
   (arrastando pro Editor, ou colocando direto na pasta pelo explorador do Windows)
3. Asset Browser detecta o .glb novo (watch de pasta ou refresh manual)
4. Import dispara:
   a. cgltf lê o .glb (geometria, materiais referenciados, hierarquia de nós)
   b. Engine converte pra estrutura interna (vértices, índices, AABB, skeleton se houver)
   c. Serializa como .kzmesh (FlatBuffers) na mesma pasta (ou em Assets/Imported/, a definir)
   d. Grava METADADO de origem dentro do .kzmesh: caminho do .glb fonte, hash do
      conteúdo do .glb no momento do import, timestamp
5. Asset Browser passa a mostrar o resultado do import (o .kzmesh), não o .glb cru
6. .glb original permanece na pasta, como referência/fonte — não é apagado
```

**Reimport (artista mudou o modelo no Blender e exportou de novo):**
```
1. Novo .glb substitui o antigo (mesmo caminho)
2. Asset Browser detecta que o hash do .glb mudou em relação ao gravado no .kzmesh
3. Ícone de "desatualizado" aparece no asset no Asset Browser
4. Dev clica "Reimportar" (ou acontece automático, a definir na Fase 4 — ver Seção 7)
5. .kzmesh é regenerado, mesmo GUID/identidade interna (entidades que já usam
   esse mesh continuam apontando pro mesmo asset, só que com geometria nova)
```

**Por que manter o GUID:** se o reimport gerasse um novo ID, toda entidade na cena que usa aquele mesh perderia a referência. O `.kzmesh` tem um identificador estável que sobrevive ao reimport.

---

## 3. Fluxo de Import — Textura

```
1. Artista exporta .png/.jpg/.tga
2. Arquivo entra em Assets/
3. Import dispara:
   a. stb_image decodifica o arquivo original
   b. DirectXTex comprime (BC1/BC3/BC5 conforme o tipo: cor, normal map, etc.)
      e gera mipmaps
   c. Serializa como asset interno de textura (mesmo padrão .kzmesh, formato
      próprio via FlatBuffers) — chamado aqui .kztex por consistência de nome
4. Original (.png/.jpg/.tga) permanece como fonte, mesma lógica do mesh
```

**Regra de mip por distância (streaming, Seção 6):** o `.kztex` guarda todos os níveis de mip gerados no import; o streaming decide em runtime quais níveis mandar pra GPU, mas a geração dos mips é feita uma vez, no import — nunca em runtime.

---

## 4. O que tem dentro do `.kzmesh` (visão conceitual do schema FlatBuffers)

Não é o schema final (isso é trabalho de implementação da Fase 4), mas o que ele **precisa** carregar pra tudo nas próximas seções funcionar:

- **GUID estável** do asset (sobrevive a reimport, renomeação de arquivo)
- Buffers de vértice/índice (já no layout de GPU, zero-copy)
- AABB (bounding box) pra culling
- Lista de **materiais referenciados** (por GUID, não por caminho — sobrevive a mover pasta)
- **Metadado de origem:** caminho do `.glb` fonte + hash do conteúdo no momento do import (usado pra detectar "desatualizado")
- Se veio de fonte: `tem_origem = true`. Se foi colocado direto sem `.glb` associado: `tem_origem = false` (ver Seção 5)

O mesmo padrão de GUID + metadado de origem vale pro `.kztex`.

---

## 5. `.kzmesh`/`.kztex` colocado direto, sem arquivo de origem

**Cenário:** alguém pega um `.kzmesh` pronto (de outro projeto Kizuri, gerado por uma ferramenta externa, ou só copiado) e solta na pasta `Assets/` sem o `.glb` correspondente.

**Comportamento decidido:**
```
1. Asset Browser detecta o .kzmesh (é um formato válido e reconhecido nativamente)
2. Lê o campo tem_origem do arquivo
3. Se tem_origem = false (ou o .glb referenciado no metadado não existe na pasta):
   → asset é aceito e funciona normalmente (pode ser arrastado pro Viewport,
     usado no Inspector, tudo igual)
   → Asset Browser mostra um indicador visual (ícone/badge) de
     "sem fonte de reimport"
   → botão "Reimportar" fica desabilitado/oculto pra esse asset
     (não tem de onde reimportar)
4. Se, depois, alguém colocar um .glb na mesma pasta com o mesmo GUID registrado
   manualmente (caso avançado, não é o fluxo padrão): fora de escopo da v1,
   tratado como edição manual por conta do usuário
```

**Por que aceitar em vez de rejeitar:** o formato é nativo da engine, não teria motivo técnico pra recusar. A única perda real é a conveniência do reimport — que fica claramente sinalizada, não escondida.

---

---

## 6. Mover/renomear o `.glb` de origem depois do import

**Cenário:** `.kzmesh` já existe e aponta pra `Assets/cubo.glb`. Depois, alguém move o `.glb` pra `Assets/Modelos/cubo.glb` (ou renomeia o arquivo). O `.kzmesh` continua funcionando normalmente pra renderizar — ele é autocontido, não precisa do `.glb` pra ser desenhado. O que quebra é só a capacidade de **reimportar**.

```
1. Import Watcher (mesmo mecanismo de watch de pasta da Seção 2) detecta uma
   operação de mover/renomear dentro de Assets/, enquanto o Editor está aberto:
   a. Se o sistema de arquivos reporta como "rename" (mesmo volume, a maioria
      dos casos no Windows): Asset Browser atualiza o caminho gravado no
      metadado do .kzmesh automaticamente, sem perguntar nada — não é uma
      mudança de conteúdo, só de local
   b. Se for reportado como delete+create separados (alguns casos de mover
      entre volumes/pastas monitoradas): cai no fluxo abaixo (passo 2)

2. Se o Editor estava FECHADO quando o arquivo foi movido (ex: você mexeu
   pelo Explorador do Windows sem o Editor aberto), na próxima vez que o
   Asset Browser tentar localizar o .glb no caminho gravado e não encontrar:
   a. .kzmesh NÃO quebra — continua funcionando, renderizando normal
   b. Estado muda pra "fonte não encontrada" (ícone próprio, diferente do
      "sem fonte" da Seção 5 — aqui já teve fonte, só está desalinhada)
   c. Engine faz uma busca automática por hash: procura em toda a pasta
      Assets/ (recursivo) por qualquer .glb cujo hash de conteúdo bata com
      o hash gravado no metadado do .kzmesh
   d. Achou por hash → reconecta automaticamente e avisa por notificação
      não-bloqueante: "cubo.kzmesh: fonte relocalizada em
      Modelos/cubo.glb" (o dev pode desfazer isso se achar que reconectou
      errado — ex: dois arquivos com conteúdo idêntico por coincidência)
   e. Não achou por hash → permanece "fonte não encontrada"; botão
      "Reimportar" fica desabilitado; dev pode clicar "Localizar fonte
      manualmente" (abre diálogo de arquivo, aponta pro .glb certo, grava
      o novo caminho)

3. Renomear/mover o .kzmesh em si (não o .glb) é o caso já coberto na Seção 7
   (Asset Browser → Renomear): GUID interno não muda, referências de outras
   entidades continuam válidas
```

**Por que buscar por hash em vez de só pelo nome:** nome pode mudar junto (ex: `cubo.glb` virou `cubo_v2.glb`), hash de conteúdo não muda só por mover/renomear. É o mesmo princípio de identidade por conteúdo já usado no KizuriMix pra realocar biblioteca quando o HD externo troca de letra.

## 7. Upload assíncrono à GPU

```
1. Import (Seções 2/3) roda no Worker/thread de import — não bloqueia o
   Editor nem o jogo
2. Asset pronto (.kzmesh/.kztex em memória) entra numa fila de staging
3. A cada frame, o Engine consome um pedaço da fila (staging buffer) e faz
   upload pra GPU — distribuído em vários frames, nunca tudo de uma vez
4. Enquanto o upload não termina, a entidade que usa esse asset mostra um
   placeholder visual (ex: cor sólida, "carregando") em vez de crashar ou
   ficar invisível
5. Upload completo → asset passa a ser desenhado normalmente
```

**Regra de tempo real (Regra 7 do TDD):** o consumo da fila de staging acontece fora da thread de áudio (não que áudio tenha relação direta aqui, mas o princípio geral de "nada bloqueante em thread crítica" também vale pra thread de render — o upload nunca trava o frame esperando o disco).

---

## 8. Asset Browser — ações do dia a dia

| Ação | O que acontece |
|---|---|
| Arrastar `.glb` de fora pra dentro do Editor | Copia pro `Assets/`, dispara import (Seção 2) |
| Arrastar `.kzmesh` de fora pra dentro do Editor | Copia pro `Assets/`, reconhecido direto (Seção 5) |
| Arrastar um asset do Browser pro Viewport | Cria uma entidade nova com `Transform` + `MeshRenderer` apontando pro GUID desse asset |
| Arrastar um asset do Browser pro campo Mesh do Inspector | Troca a malha da entidade selecionada pro GUID desse asset (sem duplicar buffer — mapa caminho/GUID→índice já carregado) |
| Botão direito → Importar | Força reimport manual (útil se o watch de pasta não pegou uma mudança) |
| Botão direito → Deletar | Ver Seção 8 (dependency tracking) |
| Botão direito → Renomear | Renomeia o arquivo; GUID interno não muda, referências continuam válidas |
| Botão direito → Mostrar na pasta do sistema | Abre o Explorador do Windows na pasta real |
| Ícone de "desatualizado" (Seção 2) | Aparece quando o hash do `.glb` fonte não bate mais com o gravado no `.kzmesh` |
| Ícone de "sem fonte de reimport" (Seção 5) | Aparece em asset colocado direto, sem `.glb`/original associado |
| Ícone de "fonte não encontrada" (Seção 6) | Já teve `.glb` vinculado, mas ele não está mais no caminho gravado; engine tenta reconectar por hash automaticamente |
| Botão direito → Localizar fonte manualmente | Só aparece com "fonte não encontrada"; abre diálogo de arquivo pra apontar o `.glb` certo |

---

## 9. Rastreamento de dependência (delete protegido)

```
1. Cada asset mantém uma contagem de quem o referencia
   (ex: Material X é usado pela Textura Y → Y tem Material X como dependente;
   Mesh Z referencia Material X → X tem Mesh Z como dependente)
2. Dev clica "Deletar" num asset que tem dependentes:
   a. Exclusão é BLOQUEADA
   b. Notificação (toast, sistema da Fase 3) aparece listando os dependentes
      por nome: "Não é possível excluir 'grama.kztex': usado por
      2 Materiais (Grama_Var1, Grama_Var2)"
3. Dev clica "Deletar" num asset SEM dependentes: exclusão normal, sem aviso
4. Dev tem a opção "Forçar exclusão" mesmo com dependentes:
   a. Asset é removido de verdade
   b. Todo dependente que apontava pra ele passa a exibir estado de
      "asset ausente" — visível na cena (ex: material rosa/placeholder de
      "textura faltando", igual ao padrão Unity/Unreal), nunca falha
      silenciosa nem crash
   c. Inspector desses dependentes mostra claramente qual referência está
      quebrada, com opção de reatribuir outro asset no lugar
```

---

## 10. Streaming de textura por distância

```
1. .kztex já vem com todos os níveis de mip (gerados no import, Seção 3)
2. Em runtime, o Engine calcula a distância da câmera até cada objeto visível
3. Objetos longe: engine pede só os mips de resolução mais baixa pra GPU
4. Objetos perto: mips de resolução mais alta são carregados
5. Ao aproximar a câmera de um objeto, o pedido de mip mais alto entra na
   mesma fila de upload assíncrono da Seção 6 — nunca trava o frame
   esperando a textura chegar; o objeto mostra o mip que já tem até o
   mip melhor terminar de subir
```

---

## 11. "Pronto quando" da Fase 4 (critério de aceite, do Roadmap)

- Um `.glb` externo é arrastado pro Editor → aparece no Asset Browser já como `.kzmesh` importado.
- Esse asset pode ser arrastado pro Viewport (cria entidade) e pro campo Mesh do Inspector (troca malha de entidade existente).
- Um `.kzmesh` solto, sem `.glb` de origem, também funciona, mas mostrado como "sem fonte de reimport".
- Editar o `.glb` original e reimportar atualiza a geometria sem quebrar entidades que já usavam aquele asset (mesmo GUID).
- Mover o `.glb` de origem pra outra subpasta não quebra o `.kzmesh`; a engine reconecta sozinha por hash ou, se não achar, avisa sem travar nada.
- Tentar deletar um asset em uso é bloqueado com notificação listando os dependentes; forçar exclusão nunca falha silenciosamente — dependentes mostram "asset ausente" de forma visível.
