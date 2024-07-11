- prometheus/client_golang ライブラリを使用しています。
- portRcvPackets という GaugeVec メトリクスを定義しています。これは、デバイス名とポート番号をラベルとして持ちます。
- collectInfiniBandMetrics 関数で、InfiniBand の情報を収集し、メトリクスを更新しています。
- メインの http.HandleFunc で /metrics エンドポイントを設定し、リクエストがあるたびに collectInfiniBandMetrics を呼び出してから、Prometheus のメトリクスハンドラを使用してレスポンスを返しています。
- サーバーは 9401 ポートでリッスンします。

## Install

```bash
go mod init sakura-exporter

go get github.com/prometheus/procfs

go build -o sakura-exporter main.go
```

新たにモジュールを追加した場合は、以下のコマンドを実行してください。

```bash
go get github.com/...
go mod tidy
```
