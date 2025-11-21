import torch
import torch.nn as nn
import torch.optim as optim
from tqdm import tqdm
import os

from dataloader import get_cifar10_loaders
from scheduler import get_scheduler

def evaluate(model, test_loader, criterion, device):
    """모델 평가"""
    model.eval()
    running_loss = 0.0
    correct = 0
    total = 0
    
    with torch.no_grad():
        for inputs, targets in tqdm(test_loader, desc="Evaluating"):
            inputs, targets = inputs.to(device), targets.to(device)
            
            # Forward pass
            outputs = model(inputs)
            loss = criterion(outputs, targets)
            
            # Statistics
            running_loss += loss.item()
            _, predicted = outputs.max(1)
            total += targets.size(0)
            correct += predicted.eq(targets).sum().item()
    
    avg_loss = running_loss / len(test_loader)
    accuracy = 100. * correct / total
    
    return avg_loss, accuracy

def train_one_epoch(model, train_loader, criterion, optimizer, device):
    """한 epoch 학습"""
    model.train()
    running_loss = 0.0
    correct = 0
    total = 0
    
    pbar = tqdm(train_loader, desc="Training")
    for inputs, targets in pbar:
        inputs, targets = inputs.to(device), targets.to(device)
        
        # Zero gradients
        optimizer.zero_grad()
        
        # Forward pass
        outputs = model(inputs)
        loss = criterion(outputs, targets)
        
        # Backward pass
        loss.backward()
        optimizer.step()
        
        # Statistics
        running_loss += loss.item()
        _, predicted = outputs.max(1)
        total += targets.size(0)
        correct += predicted.eq(targets).sum().item()
        
        # Update progress bar
        pbar.set_postfix({
            'loss': f'{running_loss/total:.4f}',
            'acc': f'{100.*correct/total:.2f}%'
        })
    
    avg_loss = running_loss / len(train_loader)
    accuracy = 100. * correct / total
    
    return avg_loss, accuracy

def train_model(model, config, save_dir=None):
    # Set random seed
    torch.manual_seed(42)
    if config["Device"] == 'cuda':
        torch.cuda.manual_seed(42)
    
    # Load dataset
    train_loader, test_loader = get_cifar10_loaders(batch_size=config["Batch_size"])
    
    model = model.to(config["Device"])
    
    # Loss and optimizer
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.SGD(model.parameters(), lr=config["Learning_rate"], 
                         momentum=0.9, weight_decay=5e-4)
    
    scheduler = get_scheduler(config["Scheduler"], config["Scheduler_param"], optimizer)
    # Training history
    
    history = {
        'train_loss': [],
        'train_acc': [],
        'test_loss': [],
        'test_acc': []
    }
    
    best_acc = 0.0
    num_epoch = config["Num_epoch"]
    for epoch in range(num_epoch):
        print(f"\nEpoch {epoch+1}/{num_epoch}")
        print("-" * 60)
        
        # Train
        train_loss, train_acc = train_one_epoch(
            model, train_loader, criterion, optimizer, config["Device"]
        )
        
        # Evaluate
        test_loss, test_acc = evaluate(
            model, test_loader, criterion, config["Device"]
        )
        
        # Update scheduler
        scheduler.step()
        
        # Save history
        history['train_loss'].append(train_loss)
        history['train_acc'].append(train_acc)
        history['test_loss'].append(test_loss)
        history['test_acc'].append(test_acc)
        
        print(f"Train Loss: {train_loss:.4f} | Train Acc: {train_acc:.2f}%")
        print(f"Test Loss: {test_loss:.4f} | Test Acc: {test_acc:.2f}%")
        print(f"Learning Rate: {scheduler.get_last_lr()[0]:.6f}")
        
        # Save best model
        if test_acc > best_acc:
            best_acc = test_acc
            if save_dir is not None:
                if not os.path.exists(save_dir):
                    os.makedirs(save_dir)
                torch.save({
                    'epoch': epoch,
                    'model_state_dict': model.state_dict(),
                    'optimizer_state_dict': optimizer.state_dict(),
                    'accuracy': best_acc,
                }, os.path.join(save_dir, f'{model.__class__.__name__}_best.pth'))
            print(f"✓ New best model saved! (Acc: {best_acc:.2f}%)")
    
    print(f"\nTraining completed! Best Test Accuracy: {best_acc:.2f}%")
    return history
