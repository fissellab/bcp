PORTS=(3000 3001 3002 3003)

for PORT in "${PORTS[@]}"; do
  echo "Killing processes on port $PORT"
  sudo lsof -i :$PORT -t | xargs -r sudo kill -9
done