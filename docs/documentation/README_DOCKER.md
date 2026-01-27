# ChronosDB Docker Deployment

## Quick Start

```bash
# Build and start
docker-compose up -d

# View logs
docker-compose logs -f

# Stop
docker-compose down
```

## Configuration

Edit `docker-compose.yml` to customize:
- Port mapping (default: 2501:2501)
- Volume mounts for data persistence
- Environment variables

## Data Persistence

Data is stored in:
- `./data` directory (mounted volume)
- Docker volume `chronosdb_data` (if using volumes)

## Health Check

The container includes a health check that verifies the server is responding on port 2501.

## Production Deployment

For production, consider:
1. Using environment variables for configuration
2. Setting up proper backup strategy for data volumes
3. Using Docker secrets for credentials
4. Configuring resource limits
5. Setting up monitoring and logging

## Example: Deploy to Cloud

### AWS ECS / Fargate
```bash
# Build and push to ECR
docker build -t chronosdb:latest .
docker tag chronosdb:latest <account>.dkr.ecr.<region>.amazonaws.com/chronosdb:latest
docker push <account>.dkr.ecr.<region>.amazonaws.com/chronosdb:latest
```

### Google Cloud Run
```bash
# Build and deploy
gcloud builds submit --tag gcr.io/<project>/chronosdb
gcloud run deploy chronosdb --image gcr.io/<project>/chronosdb --port 2501
```

### Azure Container Instances
```bash
# Build and push to ACR
az acr build --registry <registry> --image chronosdb:latest .
az container create --resource-group <rg> --name chronosdb --image <registry>.azurecr.io/chronosdb:latest --ports 2501
```
