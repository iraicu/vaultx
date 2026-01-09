# Thread test (-D 3, 1000 lookups per file)

All runs search the 11 plot files in tmp (9 single k27 plots plus merge_27_2.plot and merge_27_5.plot). Times are in milliseconds (ms). Declared flags are what was passed on the CLI; actual values are what vaultx reported in `Thread config` (outer=file-level workers, inner=record/bucket workers).

## Summary by declared vs actual threads

|Declared -t|Declared -r|Actual T|Actual R|outer|inner|Avg time/lookup (ms)|Avg total per file (ms)|Total across files (ms)|
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
|8|0|8|0|8|1|0.5154|515.39|5669.31|
|16|auto|16|0|11|1|0.5712|571.22|6283.43|
|16|0|16|0|11|1|0.5703|570.34|6273.72|
|16|4|16|4|4|4|0.4797|479.69|5276.61|
|32|auto|32|0|11|2|0.5551|555.09|6105.95|
|32|0|32|0|11|2|0.5491|549.12|6040.32|
|32|4|32|4|8|4|0.4894|489.38|5383.13|
|32|8|32|8|4|8|0.4921|492.07|5412.74|
|auto|auto|40|0|11|3|0.5829|582.89|6411.80|
|40|0|40|0|11|3|0.5265|526.47|5791.20|
|auto|4|40|4|10|4|0.5268|526.79|5794.74|
|auto|8|40|8|5|8|0.5026|502.58|5528.37|
|40|8|40|8|5|8|0.4609|460.92|5070.12|

### Declared -t 8, -r 0

- Actual: T=8, R=0, outer=8, inner=1
- Avg time/lookup across all files: 0.5154 ms
- Avg total time per file: 515.39 ms
- Total time across files: 5669.31 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5289|528.94|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5309|530.86|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5250|525.01|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5283|528.30|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5500|550.02|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5264|526.43|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5335|533.53|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5178|517.84|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.4777|477.73|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.4757|475.71|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.4749|474.92|

### Declared -t 16, -r auto

- Actual: T=16, R=0, outer=11, inner=1
- Avg time/lookup across all files: 0.5712 ms
- Avg total time per file: 571.22 ms
- Total time across files: 6283.43 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5738|573.77|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5716|571.65|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5671|567.13|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5625|562.48|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5976|597.60|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5682|568.18|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5726|572.64|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5624|562.42|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.5667|566.71|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.5702|570.23|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.5706|570.62|

### Declared -t 16, -r 0

- Actual: T=16, R=0, outer=11, inner=1
- Avg time/lookup across all files: 0.5703 ms
- Avg total time per file: 570.34 ms
- Total time across files: 6273.72 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5679|567.86|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5670|567.04|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5635|563.50|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5654|565.38|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.6028|602.83|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5639|563.87|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5779|577.94|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5613|561.34|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.5655|565.51|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.5693|569.32|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.5691|569.14|

### Declared -t 16, -r 4

- Actual: T=16, R=4, outer=4, inner=4
- Avg time/lookup across all files: 0.4797 ms
- Avg total time per file: 479.69 ms
- Total time across files: 5276.61 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.4787|478.68|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.4903|490.26|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.4853|485.25|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.4830|483.00|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5338|533.83|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.4617|461.73|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.4997|499.65|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.4620|461.98|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.4572|457.22|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.4610|460.95|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.4641|464.05|

### Declared -t 32, -r auto

- Actual: T=32, R=0, outer=11, inner=2
- Avg time/lookup across all files: 0.5551 ms
- Avg total time per file: 555.09 ms
- Total time across files: 6105.95 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5527|552.68|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5521|552.07|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5485|548.48|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5506|550.64|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5926|592.59|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5493|549.32|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5635|563.46|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5440|544.00|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.5472|547.16|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.5525|552.55|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.5530|553.00|

### Declared -t 32, -r 0

- Actual: T=32, R=0, outer=11, inner=2
- Avg time/lookup across all files: 0.5491 ms
- Avg total time per file: 549.12 ms
- Total time across files: 6040.32 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5471|547.06|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5497|549.73|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5397|539.73|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5443|544.25|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5873|587.27|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5436|543.62|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5622|562.16|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5380|538.00|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.5396|539.62|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.5450|544.99|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.5439|543.88|

### Declared -t 32, -r 4

- Actual: T=32, R=4, outer=8, inner=4
- Avg time/lookup across all files: 0.4894 ms
- Avg total time per file: 489.38 ms
- Total time across files: 5383.13 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.4968|496.84|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.4971|497.13|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.4938|493.77|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.4954|495.38|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5445|544.47|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.4957|495.66|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5193|519.26|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.4900|490.03|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.4521|452.15|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.4496|449.63|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.4488|448.82|

### Declared -t 32, -r 8

- Actual: T=32, R=8, outer=4, inner=8
- Avg time/lookup across all files: 0.4921 ms
- Avg total time per file: 492.07 ms
- Total time across files: 5412.74 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.4988|498.84|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5044|504.36|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.4997|499.74|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5009|500.88|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5365|536.50|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.4761|476.08|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5040|504.00|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.4733|473.31|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.4727|472.66|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.4738|473.78|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.4726|472.58|

### Declared -t auto, -r auto

- Actual: T=40, R=0, outer=11, inner=3
- Avg time/lookup across all files: 0.5829 ms
- Avg total time per file: 582.89 ms
- Total time across files: 6411.80 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5858|585.81|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5831|583.08|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5803|580.27|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5826|582.63|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5961|596.15|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5800|579.97|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5810|581.02|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5787|578.72|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.5794|579.37|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.5799|579.90|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.5849|584.87|

### Declared -t 40, -r 0

- Actual: T=40, R=0, outer=11, inner=3
- Avg time/lookup across all files: 0.5265 ms
- Avg total time per file: 526.47 ms
- Total time across files: 5791.20 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5173|517.28|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5189|518.86|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5165|516.48|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5146|514.61|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5929|592.88|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5169|516.86|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5467|546.74|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5130|512.95|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.5186|518.57|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.5166|516.64|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.5193|519.33|

### Declared -t auto, -r 4

- Actual: T=40, R=4, outer=10, inner=4
- Avg time/lookup across all files: 0.5268 ms
- Avg total time per file: 526.79 ms
- Total time across files: 5794.74 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5295|529.48|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5296|529.60|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5214|521.36|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5257|525.65|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5702|570.16|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.5275|527.53|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5440|543.97|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.5232|523.19|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.5231|523.07|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.5252|525.19|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.4755|475.54|

### Declared -t auto, -r 8

- Actual: T=40, R=8, outer=5, inner=8
- Avg time/lookup across all files: 0.5026 ms
- Avg total time per file: 502.58 ms
- Total time across files: 5528.37 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.5096|509.57|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.5163|516.33|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.5118|511.78|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.5132|513.23|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5625|562.51|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.4856|485.60|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.5042|504.20|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.4833|483.28|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.4820|482.03|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.4796|479.63|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.4802|480.21|

### Declared -t 40, -r 8

- Actual: T=40, R=8, outer=5, inner=8
- Avg time/lookup across all files: 0.4609 ms
- Avg total time per file: 460.92 ms
- Total time across files: 5070.12 ms

|File|Avg time/lookup (ms)|Total time (ms)|
|---|---:|---:|
|[tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot](tmp/k27-3c1b05c85b6ee696dcd84375cb3ed6440662f389cd8fb5fd1a799d6166ac74eb.plot)|0.4642|464.16|
|[tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot](tmp/k27-8a2333682e6c57139bddc3ed7a4a6eddf473210337731155fa862809634b3f41.plot)|0.4724|472.38|
|[tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot](tmp/k27-b356c73fcae9feb32a1577a31e19c5251aa6efac55ce76aa0efe700d5396d465.plot)|0.4607|460.74|
|[tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot](tmp/k27-549096d44000de894af2abc9e0b5c0466bfd3077544596b0e01d15aed0359c40.plot)|0.4677|467.70|
|[tmp/merge_27_5.plot](tmp/merge_27_5.plot)|0.5536|553.56|
|[tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot](tmp/k27-0ef0cda61d7020fc7da08f921c31a224b8a7dd995b285ceaf1fa02ad1a563a2b.plot)|0.4427|442.72|
|[tmp/merge_27_2.plot](tmp/merge_27_2.plot)|0.4765|476.47|
|[tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot](tmp/k27-7f5acdded4397ed1847dcf9a1670998a589d83906dbcac3e925d7fe43e91b770.plot)|0.4279|427.93|
|[tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot](tmp/k27-4206b631ed396aca6463baa4ab0459da6d15731160bce480b5fae9add85939bc.plot)|0.4342|434.19|
|[tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot](tmp/k27-b832f94af29a954ee11e2cee636f535c0bc9a37e37e980fe616704ac51bcdb4c.plot)|0.4323|432.34|
|[tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot](tmp/k27-7a102f0c98e61adf6f5588d3d4c4406ee3c075e9a1c4da88e064dcdd87717da2.plot)|0.4379|437.94|
