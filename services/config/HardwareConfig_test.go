package config

import (
	"os"
	"path/filepath"
	"testing"
)

func TestLoadHardwareConfig(t *testing.T) {
	tests := []struct {
		name         string
		content      *string
		wantProtocol string
		wantError    bool
	}{
		{name: "缺失文件默认 PIMS", wantProtocol: "pims"},
		{name: "缺失字段默认 PIMS", content: stringPtr(`{}`), wantProtocol: "pims"},
		{name: "规范化协议", content: stringPtr(`{"protocol":" NIMTE "}`), wantProtocol: "nimte"},
		{name: "未知协议", content: stringPtr(`{"protocol":"other"}`), wantError: true},
		{name: "损坏 JSON", content: stringPtr(`{`), wantError: true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			path := filepath.Join(t.TempDir(), "hardware.json")
			if tt.content != nil {
				if err := os.WriteFile(path, []byte(*tt.content), 0644); err != nil {
					t.Fatal(err)
				}
			}

			config, err := LoadHardwareConfig(path)
			if tt.wantError {
				if err == nil {
					t.Fatal("期望返回错误")
				}
				return
			}
			if err != nil {
				t.Fatal(err)
			}
			if config.Protocol != tt.wantProtocol {
				t.Fatalf("协议为 %q，期望 %q", config.Protocol, tt.wantProtocol)
			}
		})
	}
}

func stringPtr(value string) *string {
	return &value
}
