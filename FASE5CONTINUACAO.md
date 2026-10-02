# Fase 5 — Gráficos Adicionais (listagem)

Continuação da Fase 5, no mesmo padrão de sub-fases (5a/5b) já usado. Cada uma pequena e testável sozinha, nessa ordem de prioridade.

## 5c — SSAO (oclusão de ambiente em tela)
- Cálculo de oclusão a partir do G-Buffer de posição/normal já existente
- Blur pra tirar ruído da amostragem
- Entra na cadeia de pós-processamento, junto de Exposure/ACES/Bloom
- Toggle de ligar/desligar + intensidade/raio configuráveis

## 5d — Névoa volumétrica / raios de luz
- Reaproveita o CSM (shadow map direcional) já construído na 5b
- Ray marching pela cena, amostrando a shadow map em cada passo pra saber se o ponto está na sombra ou recebendo luz
- Densidade e cor da névoa configuráveis
- Entra na cadeia de pós-processamento, depois da iluminação normal

## 5e — Decals
- Sistema de projeção de textura sobre geometria existente (sangue, queimado, dano de parede)
- Componente/entidade de Decal: posição, tamanho, textura, tempo de vida (opcional, pra decal que some sozinho)
- Renderizado por cima da geometria já desenhada, sem duplicar mesh

## 5f — Reflexos
- Reflexo em tela (screen-space), com fallback pra quando a informação não está visível na tela
- Parâmetro de rugosidade do material controla o quão nítido/borrado é o reflexo
- Mais caro dos quatro — primeiro candidato a cortar se o tempo apertar
