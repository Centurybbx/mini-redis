package main

import (
	"flag"
	"fmt"
	"os/exec"
	"regexp"
	"strconv"
	"sync"
)

// ANSI escape codes for text color
const (
	WhiteColor = "\033[0;37m"
	GreenColor = "\033[0;32m"
	ResetColor = "\033[0m"
)

func main() {
	// Define command-line parameters
	host := flag.String("h", "localhost", "Redis server hostname")
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
	wg.Add(*ct * 2) // Each test has two parts: one for port 6379 and one for port 6996

	// Use channels to collect results of each test
	results6379 := make(chan float64, *ct)
	results6996 := make(chan float64, *ct)

	// Execute multiple benchmark tests concurrently
	for i := 0; i < *ct; i++ {
		go func() {
			defer wg.Done()

			// Build redis-benchmark command for port 6379
			args := []string{
				"-h", *host,
				"-p", "6379",
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
			results6379 <- rps
		}()

		go func() {
			defer wg.Done()

			// Build redis-benchmark command for port 6996
			args := []string{
				"-h", *host,
				"-p", "6996",
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
			results6996 <- rps
		}()
	}

	// Wait for all concurrent tests to complete
	wg.Wait()

	// Close the channels after all tests have completed
	close(results6379)
	close(results6996)

	// Print all test results
	fmt.Println("All test results:")

	// Print results in two columns with different colors
	for i := 0; i < *ct; i++ {
		rps6379 := <-results6379 // Left column (port 6379)
		rps6996 := <-results6996 // Right column (port 6996)

		fmt.Printf("%sPort 6379 (Test #%d): %.2f requests per second%s\t | %sPort 6996 (Test #%d): %.2f requests per second%s\n", WhiteColor, i+1, rps6379, ResetColor, GreenColor, i+1, rps6996, ResetColor)
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
