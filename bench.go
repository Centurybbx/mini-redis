package main

import (
	"flag"
	"fmt"
	"os/exec"
	"regexp"
	"strconv"
	"sync"
)

func main() {
	// Define command-line parameters
	host := flag.String("h", "localhost", "Redis server hostname")
	port := flag.Int("p", 6379, "Redis server port number")
	clients := flag.Int("c", 150, "Concurrent connections")
	requests := flag.Int("n", 10000, "Number of requests per connection")
	command := flag.String("t", "", "Redis command to test")
	ct := flag.Int("ct", 1, "Number of concurrent tests to execute")
	flag.Parse()

	// Check if the command parameter is empty
	if *command == "" {
		fmt.Println("Error: No Redis command specified for testing")
		return
	}

	// Use WaitGroup to wait for all concurrent tests to complete
	var wg sync.WaitGroup
	wg.Add(*ct)

	// Use channel to collect results of each test
	results := make(chan float64)

	// Execute multiple benchmark tests concurrently
	for i := 0; i < *ct; i++ {
		go func() {
			defer wg.Done()

			// Build redis-benchmark command
			args := []string{
				"-h", *host,
				"-p", fmt.Sprintf("%d", *port),
				"-c", fmt.Sprintf("%d", *clients),
				"-n", fmt.Sprintf("%d", *requests),
				"-t", *command,
			}
			cmd := exec.Command("redis-benchmark", args...)

			// Execute the command and wait for it to finish
			output, err := cmd.CombinedOutput()
			if err != nil {
				fmt.Println("Command execution error:", err)
				return
			}

			// Extract the number of requests per second
			rps := extractRequestsPerSecond(string(output))

			// Send the requests per second to the channel
			results <- rps
		}()
	}

	// Wait for all concurrent tests to complete and close the channel
	go func() {
		wg.Wait()
		close(results)
	}()

	// Print all test results
	fmt.Println("All test results:")
	for i := 0; i < *ct; i++ {
		rps := <-results
		fmt.Printf("Test #%d: %.2f requests per second\n", i+1, rps)
	}
}

// Extract the number of requests per second
func extractRequestsPerSecond(output string) float64 {
	regex := regexp.MustCompile(`(\d+\.\d+) requests per second`)
	match := regex.FindStringSubmatch(output)
	if len(match) > 1 {
		rps, _ := strconv.ParseFloat(match[1], 64)
		return rps
	}
	return 0.0
}
