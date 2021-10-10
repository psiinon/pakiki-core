package proxy

import (
	"context"
	"log"
	"net/http"
	"sync"

	"dev.forensant.com/pipeline/razor/proximitycore/proxy/request_queue"
)

var listenerWaitGroup sync.WaitGroup
var http11ProxyServer *http.Server

//RestartListeners restarts all proxy listeners, with the new addresses
func RestartListeners(settings *ProxySettings) error {
	log.Println("About to shutdown server")
	if http11ProxyServer != nil {
		err := http11ProxyServer.Shutdown(context.Background())
		if err != nil {
			return err
		}

		listenerWaitGroup.Wait()
	}

	request_queue.Close()

	log.Println("About to restart server")
	return startListenersWithConfig(settings)
}

//StartListeners starts all proxy listeners using either the default settings or the ones read from the configuration file
func StartListeners() error {
	configuration, err := GetSettings()
	if err != nil {
		return err
	}

	initConnectionPool()

	return startListenersWithConfig(configuration)
}

func startListenersWithConfig(settings *ProxySettings) error {
	listenerWaitGroup.Add(1)
	var err error
	http11ProxyServer, err = startHttp11BrowserProxy(&listenerWaitGroup, settings)
	request_queue.Init()
	if err != nil {
		listenerWaitGroup.Done()
		return err
	}
	return nil
}
