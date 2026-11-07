# Elementos do Homiservi-SCH

Elementos próprios deste fork, mantidos fora do submódulo `elements`
(a coleção oficial do QElectroTech). Eles são juntados à coleção na
instalação (`install(DIRECTORY elements_homiservi/ ...)` no CMakeLists.txt);
numa instalação manual, copie o conteúdo desta pasta para a pasta
`elements` ao lado do executável.

- `10_electric/20_manufacturers_articles/fanuc`: conectores das unidades
  FANUC (CNC, I/O e amplificadores βi SVSP), no estilo dos diagramas FANUC,
  com os cabos saindo por cima ou por baixo. Gerados pelo gerador de
  conectores do editor de elementos (estilo FANUC); confira a pinagem no
  manual FANUC da sua unidade.
